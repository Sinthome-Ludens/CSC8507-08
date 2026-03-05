#include "UI_HUD.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ── 游戏区域宽度（去掉右侧聊天面板） ──────────────────────
static float GetGameAreaWidth(float displayW) {
    return displayW - Res_ChatState::PANEL_WIDTH;
}

// ============================================================
// 1. RenderHUD_MissionPanel — 左上角任务面板
// ============================================================
static void RenderHUD_MissionPanel(ImDrawList* draw, const Res_GameState& gs, float /*gameW*/) {
    ImFont* smallFont = UITheme::GetFont_Small();
    ImFont* termFont  = UITheme::GetFont_Terminal();

    float panelX = 16.0f;
    float panelY = 12.0f;
    float panelW = 300.0f;
    float panelH = 52.0f;

    // Dark panel background
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(16, 13, 10, 180), 3.0f);

    // Mission name
    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(panelX + 10.0f, panelY + 6.0f),
        IM_COL32(252, 111, 41, 240), gs.missionName);
    if (termFont) ImGui::PopFont();

    // Objective text
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(panelX + 10.0f, panelY + 28.0f),
        IM_COL32(245, 238, 232, 200), gs.objectiveText);
    if (smallFont) ImGui::PopFont();
}

// ============================================================
// 2. RenderHUD_AlertGauge — 右上角 5 级分段彩色条
// ============================================================
static void RenderHUD_AlertGauge(ImDrawList* draw, const Res_GameState& gs, float gameW) {
    ImFont* termFont  = UITheme::GetFont_Terminal();

    float gaugeW = 220.0f;
    float gaugeH = 14.0f;
    float gaugeX = gameW - gaugeW - 20.0f;
    float gaugeY = 16.0f;
    float alertMax = (gs.alertMax > 0.001f) ? gs.alertMax : 1.0f;

    AlertStatus status = GetAlertStatus(gs.alertLevel);
    const char* statusText = GetAlertStatusText(status);

    // Status label + value
    if (termFont) ImGui::PushFont(termFont);
    char alertBuf[48];
    snprintf(alertBuf, sizeof(alertBuf), "%s %.0f / %.0f", statusText, gs.alertLevel, alertMax);
    ImVec2 alertTextSize = ImGui::CalcTextSize(alertBuf);
    draw->AddText(ImVec2(gaugeX + gaugeW - alertTextSize.x, gaugeY),
        IM_COL32(16, 13, 10, 220), alertBuf);
    if (termFont) ImGui::PopFont();

    float barY = gaugeY + 22.0f;

    // Background bar
    draw->AddRectFilled(
        ImVec2(gaugeX, barY),
        ImVec2(gaugeX + gaugeW, barY + gaugeH),
        IM_COL32(16, 13, 10, 60), 2.0f);

    // 5-segment thresholds and colors
    struct Segment { float threshold; ImU32 color; };
    Segment segments[] = {
        { 15.0f,  IM_COL32(80, 200, 120, 220) },   // Safe: green
        { 30.0f,  IM_COL32(220, 200, 0, 220) },     // Search: yellow
        { 50.0f,  IM_COL32(252, 111, 41, 220) },    // Alert: orange
        { 100.0f, IM_COL32(220, 60, 40, 220) },     // Hunt: red
        { 150.0f, IM_COL32(255, 30, 30, 220) },     // Raid: bright red
    };

    float prevThresh = 0.0f;
    for (auto& seg : segments) {
        if (gs.alertLevel <= prevThresh) break;
        float segStart = prevThresh / alertMax * gaugeW;
        float segEnd   = std::min(gs.alertLevel, seg.threshold) / alertMax * gaugeW;
        if (segEnd > segStart) {
            draw->AddRectFilled(
                ImVec2(gaugeX + segStart, barY),
                ImVec2(gaugeX + segEnd, barY + gaugeH),
                seg.color, 2.0f);
        }
        prevThresh = seg.threshold;
    }

    // Border
    draw->AddRect(
        ImVec2(gaugeX, barY),
        ImVec2(gaugeX + gaugeW, barY + gaugeH),
        IM_COL32(200, 200, 200, 100), 2.0f);

    // Segment dividers
    float dividers[] = { 15.0f, 30.0f, 50.0f, 100.0f };
    for (float d : dividers) {
        float dx = gaugeX + (d / alertMax) * gaugeW;
        draw->AddLine(ImVec2(dx, barY), ImVec2(dx, barY + gaugeH),
            IM_COL32(16, 13, 10, 80), 1.0f);
    }
}

// ============================================================
// 3. RenderHUD_Countdown — 上方中央倒计时
// ============================================================
static void RenderHUD_Countdown(ImDrawList* draw, const Res_GameState& gs, float gameW, float globalTime) {
    if (!gs.countdownActive) return;

    ImFont* titleFont = UITheme::GetFont_TerminalLarge();

    int totalSec = (int)std::max(0.0f, gs.countdownTimer);
    int mm = totalSec / 60;
    int ss = totalSec % 60;

    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mm, ss);

    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 textSize = ImGui::CalcTextSize(timeBuf);
    float cx = gameW * 0.5f - textSize.x * 0.5f;
    float cy = 14.0f;

    // Red glow when < 30s
    ImU32 textCol;
    if (gs.countdownTimer < 30.0f) {
        float pulse = (sinf(globalTime * 6.0f) + 1.0f) * 0.5f;
        uint8_t r = (uint8_t)(200 + pulse * 55);
        textCol = IM_COL32(r, 30, 30, 255);
    } else {
        textCol = IM_COL32(16, 13, 10, 240);
    }

    draw->AddText(ImVec2(cx, cy), textCol, timeBuf);
    if (titleFont) ImGui::PopFont();
}

// ============================================================
// 4. RenderHUD_PlayerState — 左下角状态标签
// ============================================================
static void RenderHUD_PlayerState(ImDrawList* draw, const Res_GameState& gs, float displayH) {
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float baseX = 20.0f;
    float baseY = displayH - 60.0f;

    const char* moveLabel;
    switch (gs.playerMoveState) {
        case PlayerMoveState::Crouching: moveLabel = "[CROUCH]"; break;
        case PlayerMoveState::Running:   moveLabel = "[SPRINT]"; break;
        default:                         moveLabel = "[STAND]";  break;
    }

    draw->AddText(ImVec2(baseX, baseY),
        IM_COL32(245, 238, 232, 200), moveLabel);

    if (gs.playerDisguised) {
        ImVec2 moveSize = ImGui::CalcTextSize(moveLabel);
        draw->AddText(ImVec2(baseX + moveSize.x + 10.0f, baseY),
            IM_COL32(80, 200, 120, 220), "[DISGUISED]");
    }

    if (termFont) ImGui::PopFont();
}

// ============================================================
// 5. RenderHUD_NoiseIndicator — 左下偏右同心环
// ============================================================
static void RenderHUD_NoiseIndicator(ImDrawList* draw, const Res_GameState& gs, float displayH, float globalTime) {
    float cx = 180.0f;
    float cy = displayH - 36.0f;
    float maxR = 20.0f;

    // Color based on noise level
    uint8_t r, g, b;
    if (gs.noiseLevel < 0.3f) {
        r = 80; g = 200; b = 120;   // green
    } else if (gs.noiseLevel < 0.6f) {
        r = 220; g = 200; b = 0;    // yellow
    } else {
        r = 220; g = 60; b = 40;    // red
    }

    // Concentric rings with pulsing
    int ringCount = 3;
    for (int i = 0; i < ringCount; ++i) {
        float phase = globalTime * 3.0f + (float)i * 0.8f;
        float expand = (fmodf(phase, 2.0f) / 2.0f);
        float radius = maxR * (0.3f + expand * 0.7f) * std::max(gs.noiseLevel, 0.1f);
        uint8_t alpha = (uint8_t)((1.0f - expand) * 180.0f * gs.noiseLevel);
        if (alpha < 5) continue;
        draw->AddCircle(ImVec2(cx, cy), radius,
            IM_COL32(r, g, b, alpha), 16, 1.5f);
    }

    // Center dot
    draw->AddCircleFilled(ImVec2(cx, cy), 3.0f,
        IM_COL32(r, g, b, (uint8_t)(180 * std::max(gs.noiseLevel, 0.15f))), 8);

    // Label
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    char noiseBuf[16];
    snprintf(noiseBuf, sizeof(noiseBuf), "%.0f%%", gs.noiseLevel * 100.0f);
    draw->AddText(ImVec2(cx + maxR + 6.0f, cy - 6.0f),
        IM_COL32(r, g, b, 200), noiseBuf);
    if (smallFont) ImGui::PopFont();
}

// ============================================================
// 6. RenderHUD_ItemSlots — 右下角装备槽
// ============================================================
static void RenderHUD_ItemSlots(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH) {
    ImFont* smallFont = UITheme::GetFont_Small();

    constexpr float kSlotW = 80.0f;
    constexpr float kSlotH = 44.0f;
    constexpr float kGap   = 6.0f;

    // 4 slots: item0, item1, weapon0, weapon1
    float totalW = 4 * kSlotW + 3 * kGap;
    float startX = gameW - totalW - 20.0f;
    float slotY  = displayH - kSlotH - 16.0f;

    struct SlotInfo {
        const char* name;
        uint8_t count;
        float cooldown;
        bool active;
    };

    SlotInfo slots[4] = {
        { gs.itemSlots[0].name,   gs.itemSlots[0].count,   gs.itemSlots[0].cooldown,   gs.activeItemSlot == 0   },
        { gs.itemSlots[1].name,   gs.itemSlots[1].count,   gs.itemSlots[1].cooldown,   gs.activeItemSlot == 1   },
        { gs.weaponSlots[0].name, gs.weaponSlots[0].count,  gs.weaponSlots[0].cooldown, gs.activeWeaponSlot == 0 },
        { gs.weaponSlots[1].name, gs.weaponSlots[1].count,  gs.weaponSlots[1].cooldown, gs.activeWeaponSlot == 1 },
    };

    if (smallFont) ImGui::PushFont(smallFont);

    for (int i = 0; i < 4; ++i) {
        float sx = startX + i * (kSlotW + kGap);
        ImVec2 slotMin(sx, slotY);
        ImVec2 slotMax(sx + kSlotW, slotY + kSlotH);

        // Background
        ImU32 bgColor = slots[i].active
            ? IM_COL32(252, 111, 41, 50)
            : IM_COL32(16, 13, 10, 120);
        draw->AddRectFilled(slotMin, slotMax, bgColor, 3.0f);

        // Border
        ImU32 borderColor = slots[i].active
            ? IM_COL32(252, 111, 41, 200)
            : IM_COL32(200, 200, 200, 80);
        draw->AddRect(slotMin, slotMax, borderColor, 3.0f, 0, slots[i].active ? 2.0f : 1.0f);

        // Name (or "---" if empty)
        const char* displayName = (slots[i].name[0] != '\0') ? slots[i].name : "---";
        draw->AddText(ImVec2(sx + 4.0f, slotY + 3.0f),
            IM_COL32(245, 238, 232, 220), displayName);

        // Count
        if (slots[i].count > 0) {
            char countBuf[8];
            snprintf(countBuf, sizeof(countBuf), "x%u", slots[i].count);
            draw->AddText(ImVec2(sx + 4.0f, slotY + 17.0f),
                IM_COL32(245, 238, 232, 160), countBuf);
        }

        // Cooldown bar
        if (slots[i].cooldown > 0.01f) {
            float cdW = kSlotW * std::clamp(slots[i].cooldown, 0.0f, 1.0f);
            draw->AddRectFilled(
                ImVec2(sx, slotY + kSlotH - 4.0f),
                ImVec2(sx + cdW, slotY + kSlotH),
                IM_COL32(200, 200, 200, 160), 0.0f);
        }

        // Slot number label
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
        ImVec2 numSize = ImGui::CalcTextSize(numBuf);
        draw->AddText(ImVec2(sx + kSlotW - numSize.x - 3.0f, slotY + 3.0f),
            IM_COL32(245, 238, 232, 100), numBuf);
    }

    if (smallFont) ImGui::PopFont();
}

// ============================================================
// 7. RenderHUD_Degradation — 全屏退化效果叠加
// ============================================================
static void RenderHUD_Degradation(ImDrawList* draw, const Res_GameState& gs, float displayW, float displayH, float globalTime) {
    float alertMax = (gs.alertMax > 0.001f) ? gs.alertMax : 1.0f;
    float alertRatio = std::clamp(gs.alertLevel / alertMax, 0.0f, 1.0f);

    // Phase 0: 0~0.2 — nothing
    if (alertRatio < 0.2f) return;

    // Phase 1: 0.2~0.4 — random noise dots
    if (alertRatio >= 0.2f) {
        float intensity = std::clamp((alertRatio - 0.2f) / 0.2f, 0.0f, 1.0f);
        int dotCount = (int)(intensity * 60);
        unsigned int seed = (unsigned int)(globalTime * 1000.0f);
        for (int i = 0; i < dotCount; ++i) {
            seed = seed * 1103515245u + 12345u;
            float nx = (float)(seed % (unsigned int)displayW);
            seed = seed * 1103515245u + 12345u;
            float ny = (float)(seed % (unsigned int)displayH);
            seed = seed * 1103515245u + 12345u;
            uint8_t a = (uint8_t)(20 + (seed % 30));
            draw->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + 2.0f, ny + 2.0f),
                IM_COL32(16, 13, 10, a));
        }
    }

    // Phase 2: 0.4~0.66 — horizontal glitch lines
    if (alertRatio >= 0.4f) {
        float intensity = std::clamp((alertRatio - 0.4f) / 0.26f, 0.0f, 1.0f);
        int lineCount = (int)(intensity * 8);
        unsigned int seed = (unsigned int)(globalTime * 500.0f);
        for (int i = 0; i < lineCount; ++i) {
            seed = seed * 1103515245u + 12345u;
            float ly = (float)(seed % (unsigned int)displayH);
            seed = seed * 1103515245u + 12345u;
            float lx = (float)(seed % (unsigned int)(displayW * 0.3f));
            seed = seed * 1103515245u + 12345u;
            float lw = 50.0f + (float)(seed % 200);
            uint8_t a = (uint8_t)(15 + intensity * 25);
            draw->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + lw, ly + 2.0f),
                IM_COL32(252, 111, 41, a));
        }
    }

    // Phase 3: 0.66~1.0 — heavy scanlines
    if (alertRatio >= 0.66f) {
        float intensity = std::clamp((alertRatio - 0.66f) / 0.34f, 0.0f, 1.0f);
        float spacing = 4.0f - intensity * 2.0f;
        uint8_t alpha = (uint8_t)(10 + intensity * 20);
        if (spacing < 2.0f) spacing = 2.0f;
        for (float y = 0.0f; y < displayH; y += spacing) {
            draw->AddLine(ImVec2(0.0f, y), ImVec2(displayW, y),
                IM_COL32(16, 13, 10, alpha), 1.0f);
        }
    }
}

// ============================================================
// RenderHUD — Main entry point
// ============================================================

void RenderHUD(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_GameState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    // playTime 累加（写操作）
    registry.ctx<Res_GameState>().playTime += dt;
    // 其余只读
    const auto& gs = registry.ctx<Res_GameState>();

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float gameW = GetGameAreaWidth(displaySize.x);
    float displayH = displaySize.y;

    // Render all 7 sub-panels
    RenderHUD_MissionPanel(draw, gs, gameW);
    RenderHUD_AlertGauge(draw, gs, gameW);
    RenderHUD_Countdown(draw, gs, gameW, ui.globalTime);
    RenderHUD_PlayerState(draw, gs, displayH);
    RenderHUD_NoiseIndicator(draw, gs, displayH, ui.globalTime);
    RenderHUD_ItemSlots(draw, gs, gameW, displayH);
    RenderHUD_Degradation(draw, gs, displaySize.x, displayH, ui.globalTime);

    // Control hints (bottom, inside game area)
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(gameW - 260.0f, displayH - 18.0f),
        IM_COL32(245, 238, 232, 100),
        "[ESC] PAUSE  [I] INVENTORY  [TAB] ITEMS");
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
