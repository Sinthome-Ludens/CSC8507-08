#include "UI_HUD.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// RenderHUD — In-game heads-up display
// ============================================================

void RenderHUD(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Try to get game state for data
    float alertLevel = 0.0f;
    uint32_t score = 0;
    uint32_t lives = 3;
    uint32_t enemies = 0;
    if (registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();
        alertLevel = gs.alertLevel;
        score      = gs.score;
        lives      = gs.playerLives;
        enemies    = gs.enemyCount;
    }

    ImFont* smallFont = UITheme::GetFont_Small();
    ImFont* termFont  = UITheme::GetFont_Terminal();

    // ── Top-left: Alert gauge ──
    {
        float gaugeX = 20.0f;
        float gaugeY = 20.0f;
        float gaugeW = 180.0f;
        float gaugeH = 8.0f;

        // Label
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(ImVec2(gaugeX, gaugeY),
            IM_COL32(16, 13, 10, 200), "ALERT LEVEL");
        if (smallFont) ImGui::PopFont();

        gaugeY += 18.0f;

        // Background bar
        draw->AddRectFilled(
            ImVec2(gaugeX, gaugeY),
            ImVec2(gaugeX + gaugeW, gaugeY + gaugeH),
            IM_COL32(200, 200, 200, 60), 2.0f);

        // Fill bar (orange intensity based on alert)
        float fillW = gaugeW * std::clamp(alertLevel, 0.0f, 1.0f);
        uint8_t alertAlpha = (uint8_t)(150 + alertLevel * 105);
        draw->AddRectFilled(
            ImVec2(gaugeX, gaugeY),
            ImVec2(gaugeX + fillW, gaugeY + gaugeH),
            IM_COL32(252, 111, 41, alertAlpha), 2.0f);

        // Border
        draw->AddRect(
            ImVec2(gaugeX, gaugeY),
            ImVec2(gaugeX + gaugeW, gaugeY + gaugeH),
            IM_COL32(200, 200, 200, 100), 2.0f);
    }

    // ── Top-right: Score ──
    {
        if (termFont) ImGui::PushFont(termFont);
        char scoreBuf[32];
        snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %u", score);
        ImVec2 scoreSize = ImGui::CalcTextSize(scoreBuf);
        draw->AddText(
            ImVec2(displaySize.x - scoreSize.x - 20.0f, 20.0f),
            IM_COL32(16, 13, 10, 220), scoreBuf);
        if (termFont) ImGui::PopFont();
    }

    // ── Bottom-left: Lives / Player status ──
    {
        if (smallFont) ImGui::PushFont(smallFont);
        char livesBuf[32];
        snprintf(livesBuf, sizeof(livesBuf), "LIVES: %u", lives);
        draw->AddText(
            ImVec2(20.0f, displaySize.y - 60.0f),
            IM_COL32(16, 13, 10, 200), livesBuf);

        char enemyBuf[32];
        snprintf(enemyBuf, sizeof(enemyBuf), "HOSTILES: %u", enemies);
        draw->AddText(
            ImVec2(20.0f, displaySize.y - 40.0f),
            IM_COL32(16, 13, 10, 180), enemyBuf);
        if (smallFont) ImGui::PopFont();
    }

    // ── Bottom-center: Item slots (4 slots) ──
    {
        constexpr int kSlotCount = 4;
        constexpr float kSlotSize = 40.0f;
        constexpr float kSlotGap  = 6.0f;
        float totalW = kSlotCount * kSlotSize + (kSlotCount - 1) * kSlotGap;
        float startX = (displaySize.x - totalW) * 0.5f;
        float slotY  = displaySize.y - kSlotSize - 16.0f;

        for (int i = 0; i < kSlotCount; ++i) {
            float sx = startX + i * (kSlotSize + kSlotGap);
            ImVec2 slotMin(sx, slotY);
            ImVec2 slotMax(sx + kSlotSize, slotY + kSlotSize);

            // Slot background
            draw->AddRectFilled(slotMin, slotMax,
                IM_COL32(245, 238, 232, 180), 3.0f);
            draw->AddRect(slotMin, slotMax,
                IM_COL32(200, 200, 200, 120), 3.0f);

            // Slot number
            if (smallFont) ImGui::PushFont(smallFont);
            char numBuf[4];
            snprintf(numBuf, sizeof(numBuf), "%d", i + 1);
            draw->AddText(ImVec2(sx + 3.0f, slotY + 2.0f),
                IM_COL32(16, 13, 10, 120), numBuf);
            if (smallFont) ImGui::PopFont();
        }
    }

    // ── Bottom-right: Noise indicator ──
    {
        if (smallFont) ImGui::PushFont(smallFont);
        float noiseX = displaySize.x - 120.0f;
        float noiseY = displaySize.y - 40.0f;

        draw->AddText(ImVec2(noiseX, noiseY),
            IM_COL32(16, 13, 10, 180), "NOISE: LOW");

        // Small noise bar
        float barX = noiseX;
        float barY = noiseY - 12.0f;
        float barW = 100.0f;
        float barH = 4.0f;
        draw->AddRectFilled(
            ImVec2(barX, barY),
            ImVec2(barX + barW, barY + barH),
            IM_COL32(200, 200, 200, 60), 1.0f);
        draw->AddRectFilled(
            ImVec2(barX, barY),
            ImVec2(barX + barW * 0.2f, barY + barH),
            IM_COL32(252, 111, 41, 160), 1.0f);

        if (smallFont) ImGui::PopFont();
    }

    // ── Top-center: Mission panel ──
    {
        if (smallFont) ImGui::PushFont(smallFont);
        const char* mission = "OBJECTIVE: INFILTRATE TARGET";
        ImVec2 missionSize = ImGui::CalcTextSize(mission);
        float mx = (displaySize.x - missionSize.x) * 0.5f;
        draw->AddText(ImVec2(mx, 20.0f),
            IM_COL32(16, 13, 10, 180), mission);
        if (smallFont) ImGui::PopFont();
    }

    // ── Control hints ──
    {
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(
            ImVec2(displaySize.x - 200.0f, displaySize.y - 20.0f),
            IM_COL32(16, 13, 10, 120),
            "[ESC] PAUSE  [I] INVENTORY  [TAB] ITEMS");
        if (smallFont) ImGui::PopFont();
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
