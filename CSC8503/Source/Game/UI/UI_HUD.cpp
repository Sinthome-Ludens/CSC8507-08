/**
 * @file UI_HUD.cpp
 * @brief HUD 渲染实现：任务面板、警戒条、倒计时、玩家状态、噪音、装备槽、
 *        降级特效、多人对战面板（对手进度条/干扰特效/网络状态）及 RenderHUD 入口。
 *
 * @details
 * 所有子面板均为 static 函数，仅由 RenderHUD() 调用。
 * 使用 ImDrawList（前景层）绘制，不产生 ImGui 窗口。
 *
 * @see UI_HUD.h, UI_ActionNotify.h（动作通知卡片独立渲染）
 */
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
#include "Game/UI/UI_ItemIcons.h"

namespace ECS::UI {

/// @brief 返回去掉右侧聊天面板后的游戏区域宽度（像素）。
static float GetGameAreaWidth(float displayW) {
    return displayW - Res_ChatState::kPanelWidth;
}

// ============================================================
// 1. RenderHUD_MissionPanel — 左上角任务面板
// ============================================================
/// @brief 渲染左上角任务面板（任务名 + 目标文字）。
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
// 2. RenderHUD_AlertGauge — 右上角 4 级分段彩色条
// ============================================================
/// @brief 渲染右上角警戒条（Safe/Search/Alert/Hunt 四级分段彩色条）。倒计时激活时隐藏。
static void RenderHUD_AlertGauge(ImDrawList* draw, const Res_GameState& gs, float gameW) {
    // 倒计时激活时隐藏警戒条
    if (gs.countdownActive) return;

    ImFont* termFont  = UITheme::GetFont_Terminal();

    float gaugeW = 220.0f;
    float gaugeH = 14.0f;
    float gaugeX = gameW - gaugeW - 20.0f;
    float gaugeY = 16.0f;
    float alertMax = (gs.alertMax > 0.001f) ? gs.alertMax : 1.0f;

    AlertStatus status = GetAlertStatus(gs.alertLevel);

    // 彩色数字 "XX/100"，颜色随 AlertStatus 变化
    ImU32 numCol;
    switch (status) {
        case AlertStatus::Safe:   numCol = IM_COL32(80, 200, 120, 220);  break; // green
        case AlertStatus::Search: numCol = IM_COL32(220, 200, 0, 220);   break; // yellow
        case AlertStatus::Alert:  numCol = IM_COL32(252, 111, 41, 220);  break; // orange
        case AlertStatus::Hunt:   numCol = IM_COL32(220, 60, 40, 220);   break; // red
        default:                  numCol = IM_COL32(16, 13, 10, 220);    break;
    }

    if (termFont) ImGui::PushFont(termFont);
    char alertBuf[16];
    snprintf(alertBuf, sizeof(alertBuf), "%.0f/%.0f", gs.alertLevel, alertMax);
    ImVec2 alertTextSize = ImGui::CalcTextSize(alertBuf);
    draw->AddText(ImVec2(gaugeX + gaugeW - alertTextSize.x, gaugeY), numCol, alertBuf);
    if (termFont) ImGui::PopFont();

    float barY = gaugeY + 22.0f;

    // Background bar
    draw->AddRectFilled(
        ImVec2(gaugeX, barY),
        ImVec2(gaugeX + gaugeW, barY + gaugeH),
        IM_COL32(16, 13, 10, 60), 2.0f);

    // 4-segment thresholds and colors (no Raid)
    struct Segment { float threshold; ImU32 color; };
    Segment segments[] = {
        { 25.0f,  IM_COL32(80, 200, 120, 220) },   // Safe: green
        { 50.0f,  IM_COL32(220, 200, 0, 220) },     // Search: yellow
        { 75.0f,  IM_COL32(252, 111, 41, 220) },    // Alert: orange
        { 100.0f, IM_COL32(220, 60, 40, 220) },     // Hunt: red
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

}

// ============================================================
// 2b. RenderHUD_Score — 警戒条下方积分 + 评级
// ============================================================
/// @brief 渲染警戒条下方积分 + 实时评级（如 "SCORE: 850  [A]"）。
static void RenderHUD_Score(ImDrawList* draw, int32_t score, float gameW) {
    ImFont* termFont = UITheme::GetFont_Terminal();

    const char* rating = GetScoreRating(score);
    int8_t tier = GetScoreRatingTier(score);

    const ImU32 scoreCol = UITheme::GetScoreRatingColor(tier, 220);

    if (termFont) ImGui::PushFont(termFont);

    char scoreBuf[24];
    snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %d", std::max(0, score));
    ImVec2 scoreSize = ImGui::CalcTextSize(scoreBuf);

    char ratingBuf[8];
    snprintf(ratingBuf, sizeof(ratingBuf), "[%s]", rating);
    ImVec2 ratingSize = ImGui::CalcTextSize(ratingBuf);

    float rightEdge = gameW - 20.0f;
    float scoreY = 60.0f;  // 警戒条 bar 底部下方

    float totalW = scoreSize.x + 8.0f + ratingSize.x;
    float startX = rightEdge - totalW;

    draw->AddText(ImVec2(startX, scoreY), scoreCol, scoreBuf);
    draw->AddText(ImVec2(startX + scoreSize.x + 8.0f, scoreY),
                  scoreCol, ratingBuf);

    if (termFont) ImGui::PopFont();
}

// ============================================================
// 3. RenderHUD_Countdown — 上方中央倒计时
// ============================================================
/// @brief 渲染上方居中倒计时（MM:SS），仅在 countdownActive 时显示，脉冲动画随 globalTime。
static void RenderHUD_Countdown(ImDrawList* draw, const Res_GameState& gs, float gameW, float globalTime) {
    if (!gs.countdownActive) return;

    ImFont* titleFont = UITheme::GetFont_TerminalLarge();

    int totalSec = (int)std::max(0.0f, gs.countdownTimer);

    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "!%d!", totalSec);

    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 textSize = ImGui::CalcTextSize(timeBuf);
    float cx = gameW * 0.5f - textSize.x * 0.5f;
    float cy = 14.0f + textSize.y * 2.0f;  // 下移两个文字高度

    // 红色脉冲（加亮：基础 230, 绿/蓝 60）
    float pulse = (sinf(globalTime * 6.0f) + 1.0f) * 0.5f;
    uint8_t r = (uint8_t)(230 + pulse * 25);
    ImU32 textCol  = IM_COL32(r, 60, 60, 255);
    ImU32 shadowCol = IM_COL32(80, 0, 0, 180);

    // 描边模拟加粗（8方向偏移1px）
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            draw->AddText(ImVec2(cx + dx, cy + dy), shadowCol, timeBuf);
        }
    }
    // 主文本
    draw->AddText(ImVec2(cx, cy), textCol, timeBuf);
    if (titleFont) ImGui::PopFont();
}

// ============================================================
// 4. RenderHUD_PlayerState — 左下角状态标签
// ============================================================
/// @brief 渲染左下角玩家状态标签（STAND/CROUCH/RUN + DISGUISED）。
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
/// @brief 渲染噪音同心环指示器（脉动环数随 noiseLevel 变化，globalTime 驱动动画）。
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
// 6. RenderHUD_ItemSlots — 底部居中双面板装备栏
// ============================================================
/// @brief 渲染底部居中装备栏（左=激活道具 Q，右=激活武器 E），含图标/冷却/闪光。
static void RenderHUD_ItemSlots(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH) {
    ImFont* smallFont = UITheme::GetFont_Small();
    ImFont* termFont  = UITheme::GetFont_Terminal();

    constexpr float kPanelW = 140.0f;
    constexpr float kPanelH = 60.0f;
    constexpr float kGap    = 10.0f;
    constexpr float kIconSize = 12.0f;

    float totalW = 2 * kPanelW + kGap;
    float startX = gameW * 0.5f - totalW * 0.5f;
    float panelY = displayH - kPanelH - 24.0f;

    struct PanelData {
        const SlotDisplay* slot;
        const char* keyLabel;
        const char* typeLabel;
        bool isFlashing;
    };

    const uint8_t safeItemSlot   = (gs.activeItemSlot   < 2) ? gs.activeItemSlot   : 0;
    const uint8_t safeWeaponSlot = (gs.activeWeaponSlot < 2) ? gs.activeWeaponSlot : 0;

    PanelData panels[2] = {
        { &gs.itemSlots[safeItemSlot],     "[Q]", "GADGET",
          gs.itemUseFlashTimer > 0.0f && gs.itemUseFlashSlotType == 0 },
        { &gs.weaponSlots[safeWeaponSlot], "[E]", "WEAPON",
          gs.itemUseFlashTimer > 0.0f && gs.itemUseFlashSlotType == 1 },
    };

    for (int i = 0; i < 2; ++i) {
        float px = startX + i * (kPanelW + kGap);
        ImVec2 panelMin(px, panelY);
        ImVec2 panelMax(px + kPanelW, panelY + kPanelH);

        const auto* slot = panels[i].slot;
        bool hasItem = (slot->name[0] != '\0');
        bool onCooldown = (slot->cooldown > 0.01f);

        // Background
        draw->AddRectFilled(panelMin, panelMax, IM_COL32(16, 13, 10, 140), 3.0f);

        // Border (flash = bright orange, normal = orange, empty = gray)
        if (panels[i].isFlashing) {
            uint8_t flashAlpha = (uint8_t)(200 + 55 * gs.itemUseFlashTimer / 0.3f);
            draw->AddRect(panelMin, panelMax, IM_COL32(252, 111, 41, flashAlpha), 3.0f, 0, 3.0f);
        } else if (hasItem) {
            draw->AddRect(panelMin, panelMax, IM_COL32(252, 111, 41, 200), 3.0f, 0, 1.5f);
        } else {
            // Empty: dashed appearance (draw 4 corner segments)
            draw->AddRect(panelMin, panelMax, IM_COL32(200, 200, 200, 60), 3.0f, 0, 1.0f);
        }

        if (hasItem) {
            // Icon (left side)
            ImVec2 iconCenter(px + 20.0f, panelY + 20.0f);
            if (slot->itemId < static_cast<uint8_t>(ItemID::Count)) {
                DrawItemIcon(draw, iconCenter, kIconSize, static_cast<ItemID>(slot->itemId),
                    IM_COL32(245, 238, 232, 220));
            }

            // Name (right of icon)
            if (termFont) ImGui::PushFont(termFont);
            draw->AddText(ImVec2(px + 38.0f, panelY + 4.0f),
                IM_COL32(245, 238, 232, 220), slot->name);
            if (termFont) ImGui::PopFont();

            // Count + status
            if (smallFont) ImGui::PushFont(smallFont);
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), "x%u", slot->count);
            draw->AddText(ImVec2(px + 38.0f, panelY + 20.0f),
                IM_COL32(252, 111, 41, 200), countBuf);

            // READY / COOLDOWN label
            ImVec2 countSize = ImGui::CalcTextSize(countBuf);
            if (onCooldown) {
                draw->AddText(ImVec2(px + 38.0f + countSize.x + 8.0f, panelY + 20.0f),
                    IM_COL32(220, 60, 40, 200), "WAIT");
            } else if (slot->count > 0) {
                draw->AddText(ImVec2(px + 38.0f + countSize.x + 8.0f, panelY + 20.0f),
                    IM_COL32(80, 200, 120, 200), "READY");
            }
            if (smallFont) ImGui::PopFont();

            // Cooldown progress bar (bottom edge)
            if (onCooldown) {
                float cdFill = std::clamp(slot->cooldown, 0.0f, 1.0f);
                float barW = kPanelW * cdFill;
                draw->AddRectFilled(
                    ImVec2(px, panelY + kPanelH - 4.0f),
                    ImVec2(px + barW, panelY + kPanelH),
                    IM_COL32(220, 60, 40, 180), 0.0f);
            } else {
                // Full green bar when ready
                draw->AddRectFilled(
                    ImVec2(px, panelY + kPanelH - 4.0f),
                    ImVec2(px + kPanelW, panelY + kPanelH),
                    IM_COL32(80, 200, 120, 100), 0.0f);
            }
        } else {
            // Empty slot
            if (smallFont) ImGui::PushFont(smallFont);
            draw->AddText(ImVec2(px + kPanelW * 0.5f - 10.0f, panelY + kPanelH * 0.5f - 6.0f),
                IM_COL32(200, 200, 200, 100), "---");
            if (smallFont) ImGui::PopFont();
        }

        // Key label + type label (bottom)
        if (smallFont) ImGui::PushFont(smallFont);
        char labelBuf[24];
        snprintf(labelBuf, sizeof(labelBuf), "%s %s", panels[i].keyLabel, panels[i].typeLabel);
        ImVec2 labelSize = ImGui::CalcTextSize(labelBuf);
        draw->AddText(ImVec2(px + kPanelW * 0.5f - labelSize.x * 0.5f, panelY + kPanelH - 16.0f),
            IM_COL32(245, 238, 232, 120), labelBuf);
        if (smallFont) ImGui::PopFont();
    }
}

// ============================================================
// 7. RenderHUD_Degradation — 全屏退化效果叠加
// ============================================================
/// @brief 渲染随警戒等级加深的全屏退化效果（边缘发红 + 扫描线 + 噪点闪烁）。
static void RenderHUD_Degradation(ImDrawList* draw, const Res_GameState& gs, float displayW, float displayH, float globalTime) {
    float alertMax = (gs.alertMax > 0.001f) ? gs.alertMax : 1.0f;
    float alertRatio = std::clamp(gs.alertLevel / alertMax, 0.0f, 1.0f);

    // Phase 0: 0~0.3 — 无效果
    if (alertRatio < 0.3f) return;

    // Phase 1: 0.3~0.6 — 噪点
    if (alertRatio >= 0.3f) {
        float intensity = std::clamp((alertRatio - 0.3f) / 0.3f, 0.0f, 1.0f);
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

    // Phase 2: 0.6~1.0 — 水平干扰线 + 扫描线
    if (alertRatio >= 0.6f) {
        float intensity = std::clamp((alertRatio - 0.6f) / 0.4f, 0.0f, 1.0f);

        // 水平干扰线
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

        // 扫描线
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
// 8. RenderHUD_MatchBanner — 多人状态横幅
// ============================================================
/// @brief 渲染上方居中的多人状态横幅（等待对手 / 游戏开始）。
static void RenderHUD_MatchBanner(ImDrawList* draw, const Res_GameState& gs, float gameW, float globalTime, float dt) {
    static float sMatchStartBannerTimer = 0.0f;
    if (gs.matchJustStarted) {
        sMatchStartBannerTimer = 2.2f;
    } else if (sMatchStartBannerTimer > 0.0f) {
        sMatchStartBannerTimer = std::max(0.0f, sMatchStartBannerTimer - dt);
    }

    const bool showWaiting = gs.matchPhase == MatchPhase::WaitingForPeer;
    const bool showStart = gs.matchPhase == MatchPhase::Running && sMatchStartBannerTimer > 0.0f;
    if (!showWaiting && !showStart) return;

    const char* title = showWaiting ? "WAITING FOR OPPONENT" : "MATCH START";
    const char* subtitle = showWaiting ? "STANDING BY FOR CONNECTION" : "MOVE OUT";

    ImFont* titleFont = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Terminal();

    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    if (titleFont) ImGui::PopFont();

    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
    if (smallFont) ImGui::PopFont();

    const float padX = 30.0f;
    const float padTop = 16.0f;
    const float padBottom = 14.0f;
    const float boxW = std::max(titleSize.x, subtitleSize.x) + padX * 2.0f;
    const float boxH = titleSize.y + subtitleSize.y + padTop + padBottom + 12.0f;
    const float boxX = gameW * 0.5f - boxW * 0.5f;
    const float boxY = gs.countdownActive ? 136.0f : 96.0f;

    float fade = 1.0f;
    if (showStart) {
        fade = std::min(1.0f, sMatchStartBannerTimer / 0.35f);
        fade = std::min(fade, sMatchStartBannerTimer / 2.2f + 0.25f);
    }

    const float pulse = showWaiting ? (sinf(globalTime * 2.8f) + 1.0f) * 0.5f : 1.0f;
    const ImU32 bgCol = IM_COL32(16, 13, 10, static_cast<int>(160.0f * fade));
    const ImU32 borderCol = showWaiting
        ? IM_COL32(200, 200, 200, static_cast<int>((120.0f + pulse * 60.0f) * fade))
        : IM_COL32(252, 111, 41, static_cast<int>(190.0f * fade));
    const ImU32 accentCol = IM_COL32(252, 111, 41, static_cast<int>((showWaiting ? (205.0f + pulse * 30.0f) : 235.0f) * fade));
    const ImU32 subtitleCol = IM_COL32(245, 238, 232, static_cast<int>(190.0f * fade));

    draw->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), bgCol, 5.0f);
    draw->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), borderCol, 5.0f, 0, 1.4f);
    draw->AddLine(ImVec2(boxX + 18.0f, boxY + 38.0f), ImVec2(boxX + boxW - 18.0f, boxY + 38.0f),
        IM_COL32(200, 200, 200, static_cast<int>(82.0f * fade)), 1.0f);

    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(gameW * 0.5f - titleSize.x * 0.5f, boxY + padTop), accentCol, title);
    if (titleFont) ImGui::PopFont();

    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(gameW * 0.5f - subtitleSize.x * 0.5f, boxY + padTop + titleSize.y + 10.0f),
        subtitleCol, subtitle);
    if (smallFont) ImGui::PopFont();
}

// ============================================================
// 9. RenderHUD_OpponentBar — 多人对战对手进度条
// ============================================================
/// @brief 渲染上方中央的对手三段式进度条，仅在多人比赛开始后显示。
static void RenderHUD_OpponentBar(ImDrawList* draw, const Res_GameState& gs, float gameW) {
    if (gs.matchPhase == MatchPhase::WaitingForPeer || gs.matchPhase == MatchPhase::Starting) {
        return;
    }

    ImFont* titleFont = UITheme::GetFont_Terminal();

    constexpr float barW = 340.0f;
    constexpr float barH = 14.0f;
    constexpr float segmentGap = 4.0f;
    float barX = gameW * 0.5f - barW * 0.5f;
    float barY = gs.countdownActive ? 78.0f : 56.0f;

    const uint8_t opponentStage = std::min<uint8_t>(gs.opponentStageProgress, kMultiplayerStageCount);
    const float segmentW = (barW - segmentGap * (kMultiplayerStageCount - 1)) / static_cast<float>(kMultiplayerStageCount);

    const ImU32 inactiveCol = IM_COL32(125, 125, 125, 105);
    const ImU32 stageCols[kMultiplayerStageCount] = {
        IM_COL32(118, 154, 109, 225),
        IM_COL32(252, 111, 41, 225),
        IM_COL32(220, 60, 40, 225),
    };

    // Title above the bar
    if (titleFont) ImGui::PushFont(titleFont);
    const char* title = "OPPONENT PROGRESS";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    draw->AddText(ImVec2(gameW * 0.5f - titleSize.x * 0.5f, barY - 24.0f),
        IM_COL32(220, 60, 40, 220), title);
    if (titleFont) ImGui::PopFont();

    // Single segmented opponent bar
    for (uint8_t i = 0; i < kMultiplayerStageCount; ++i) {
        const float segX = barX + i * (segmentW + segmentGap);
        const ImVec2 oppMin(segX, barY);
        const ImVec2 oppMax(segX + segmentW, barY + barH);
        const ImU32 oppCol = (i < opponentStage) ? stageCols[i] : inactiveCol;

        draw->AddRectFilled(oppMin, oppMax, oppCol, 2.0f);
        draw->AddRect(oppMin, oppMax, IM_COL32(200, 200, 200, 95), 2.0f, 0, 1.0f);
    }
}

// ============================================================
// 10. RenderHUD_DisruptionEffect — 干扰效果全屏叠加
// ============================================================
/// @brief 渲染对手干扰效果（视觉干扰/减速/信号扰乱），仅在 disruptionType != 0 时生效。
static void RenderHUD_DisruptionEffect(ImDrawList* draw, const Res_GameState& gs,
                                        float displayW, float displayH, float globalTime) {
    if (gs.disruptionType == 0 || gs.disruptionTimer <= 0.0f) return;

    float progress = (gs.disruptionDuration > 0.001f)
        ? std::clamp(gs.disruptionTimer / gs.disruptionDuration, 0.0f, 1.0f)
        : 0.0f;

    switch (gs.disruptionType) {
        case 1: {
            // 视觉干扰 — 边缘红色脉冲 + 闪烁
            float pulse = (sinf(globalTime * 8.0f) + 1.0f) * 0.5f;
            uint8_t alpha = (uint8_t)(40.0f + pulse * 60.0f * progress);

            // Top edge
            draw->AddRectFilledMultiColor(
                ImVec2(0, 0), ImVec2(displayW, 40.0f),
                IM_COL32(220, 40, 40, alpha), IM_COL32(220, 40, 40, alpha),
                IM_COL32(220, 40, 40, 0), IM_COL32(220, 40, 40, 0));
            // Bottom edge
            draw->AddRectFilledMultiColor(
                ImVec2(0, displayH - 40.0f), ImVec2(displayW, displayH),
                IM_COL32(220, 40, 40, 0), IM_COL32(220, 40, 40, 0),
                IM_COL32(220, 40, 40, alpha), IM_COL32(220, 40, 40, alpha));
            break;
        }
        case 2: {
            // 减速干扰 — 蓝色网格叠加
            uint8_t alpha = (uint8_t)(20.0f * progress);
            float spacing = 30.0f;
            for (float x = 0; x < displayW; x += spacing) {
                draw->AddLine(ImVec2(x, 0), ImVec2(x, displayH),
                    IM_COL32(60, 100, 220, alpha), 1.0f);
            }
            for (float y = 0; y < displayH; y += spacing) {
                draw->AddLine(ImVec2(0, y), ImVec2(displayW, y),
                    IM_COL32(60, 100, 220, alpha), 1.0f);
            }
            break;
        }
        case 3: {
            // 信号扰乱 — 随机色块闪烁
            unsigned int seed = (unsigned int)(globalTime * 2000.0f);
            int blockCount = (int)(15.0f * progress);
            for (int i = 0; i < blockCount; ++i) {
                seed = seed * 1103515245u + 12345u;
                float bx = (float)(seed % (unsigned int)displayW);
                seed = seed * 1103515245u + 12345u;
                float by = (float)(seed % (unsigned int)displayH);
                seed = seed * 1103515245u + 12345u;
                float bw = 20.0f + (float)(seed % 80);
                seed = seed * 1103515245u + 12345u;
                float bh = 2.0f + (float)(seed % 8);
                uint8_t a = (uint8_t)(30 + (seed % 40));
                draw->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                    IM_COL32(252, 111, 41, a));
            }
            break;
        }
        default: break;
    }

    // "DISRUPTED" indicator
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* labels[] = { "", "VISION IMPAIRED", "SPEED REDUCED", "SIGNAL SCRAMBLED" };
    const char* label = (gs.disruptionType < 4) ? labels[gs.disruptionType] : "";
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    float labelX = displayW * 0.5f - labelSize.x * 0.5f;

    float blink = (sinf(globalTime * 6.0f) + 1.0f) * 0.5f;
    uint8_t lblAlpha = (uint8_t)(150 + blink * 105);
    draw->AddText(ImVec2(labelX, 70.0f), IM_COL32(220, 40, 40, lblAlpha), label);

    if (termFont) ImGui::PopFont();
}

// ============================================================
// 11. RenderHUD_NetworkStatus — 右下角网络状态
// ============================================================
/// @brief 渲染右下角网络状态（PING 值，颜色随延迟变化），仅多人模式调用。
static void RenderHUD_NetworkStatus(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH) {
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    char pingBuf[24];
    snprintf(pingBuf, sizeof(pingBuf), "PING: %ums", gs.networkPing);

    // Color based on ping quality
    ImU32 pingCol;
    if (gs.networkPing < 50)       pingCol = IM_COL32(80, 200, 120, 180);   // green
    else if (gs.networkPing < 100) pingCol = IM_COL32(220, 200, 0, 180);    // yellow
    else                           pingCol = IM_COL32(220, 60, 40, 180);    // red

    ImVec2 pingSize = ImGui::CalcTextSize(pingBuf);
    draw->AddText(
        ImVec2(gameW - pingSize.x - 20.0f, displayH - 78.0f),
        pingCol, pingBuf);

    if (smallFont) ImGui::PopFont();
}


// ============================================================
// RenderHUD — Main entry point
// ============================================================
/**
 * @brief HUD 渲染入口：按顺序调用所有子面板渲染函数（任务/警戒/倒计时/玩家状态/噪音/装备/多人）。
 *        由 Sys_UI::OnUpdate 在 UIScreen::HUD 状态下调用。
 * @param registry ECS 注册表（读取 Res_UIState、Res_GameState）
 * @param dt       帧时间（秒）
 */

void RenderHUD(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_GameState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    // playTime 由 Sys_UI 累加，此处只读
    const auto& gs = registry.ctx<Res_GameState>();

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float gameW = GetGameAreaWidth(displaySize.x);
    float displayH = displaySize.y;

    // Render all sub-panels (7 base + 3 multiplayer)
    RenderHUD_MissionPanel(draw, gs, gameW);
    RenderHUD_AlertGauge(draw, gs, gameW);
    if (!gs.isMultiplayer) {
        RenderHUD_Score(draw, ui.campaignScore, gameW);
    }
    RenderHUD_Countdown(draw, gs, gameW, ui.globalTime);
    RenderHUD_PlayerState(draw, gs, displayH);
    RenderHUD_NoiseIndicator(draw, gs, displayH, ui.globalTime);
    RenderHUD_ItemSlots(draw, gs, gameW, displayH);
    RenderHUD_Degradation(draw, gs, displaySize.x, displayH, ui.globalTime);

    // Multiplayer-only panels
    if (gs.isMultiplayer) {
        RenderHUD_MatchBanner(draw, gs, gameW, ui.globalTime, dt);
        RenderHUD_OpponentBar(draw, gs, gameW);
        RenderHUD_DisruptionEffect(draw, gs, displaySize.x, displayH, ui.globalTime);
        RenderHUD_NetworkStatus(draw, gs, gameW, displayH);
    }

    // Control hints (bottom, inside game area)
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    const char* hints = "[ESC] PAUSE  [Q] GADGET  [E] WEAPON  [TAB] SWITCH  [I] INVENTORY";
    ImVec2 hintsSize = ImGui::CalcTextSize(hints);
    draw->AddText(
        ImVec2(gameW * 0.5f - hintsSize.x * 0.5f, displayH - 18.0f),
        IM_COL32(245, 238, 232, 100), hints);
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
