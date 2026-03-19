/**
 * @file UI_HUD_Status.cpp
 * @brief HUD sub-module: player state tags + noise indicator + countdown timer.
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

void PlayerState(ImDrawList* draw, const Res_GameState& gs, float displayH) {
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float baseX = 20.0f;
    float baseY = displayH - 60.0f;

    const char* moveLabel;
    switch (gs.playerMoveState) {
        case PlayerMoveState::Crouching: moveLabel = "[CROUCH]"; break;
        case PlayerMoveState::Running:   moveLabel = "[SPRINT]"; break;
        default:                         moveLabel = "[STAND]";  break;
    }

    draw->AddText(ImVec2(baseX, baseY), Col32_Bg(200), moveLabel);

    if (gs.playerDisguised) {
        ImVec2 moveSize = ImGui::CalcTextSize(moveLabel);
        draw->AddText(ImVec2(baseX + moveSize.x + 10.0f, baseY),
            IM_COL32(80, 200, 120, 220), "[DISGUISED]");  // game state color
    }

    if (termFont) ImGui::PopFont();
}

void NoiseIndicator(ImDrawList* draw, const Res_GameState& gs, float displayH, float globalTime) {
    float cx = 180.0f;
    float cy = displayH - 36.0f;
    float maxR = 20.0f;

    // Color based on noise level (game mechanic — dynamic RGB)
    uint8_t r, g, b;
    if (gs.noiseLevel < 0.3f) {
        r = 80; g = 200; b = 120;
    } else if (gs.noiseLevel < 0.6f) {
        r = 220; g = 200; b = 0;
    } else {
        r = 220; g = 60; b = 40;
    }

    // Background circle
    draw->AddCircleFilled(ImVec2(cx, cy), maxR + 6.0f,
        Col32_BgDark(80), 24);

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

    // "NOISE" label above
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(cx - 14.0f, cy - maxR - 16.0f),
        Col32_Bg(140), "NOISE");
    // Percentage label to the right
    char noiseBuf[16];
    snprintf(noiseBuf, sizeof(noiseBuf), "%.0f%%", gs.noiseLevel * 100.0f);
    draw->AddText(ImVec2(cx + maxR + 6.0f, cy - 6.0f),
        IM_COL32(r, g, b, 200), noiseBuf);
    if (smallFont) ImGui::PopFont();
}

void Countdown(ImDrawList* draw, const Res_GameState& gs, float gameW, float globalTime) {
    if (!gs.countdownActive) return;

    ImFont* titleFont = GetFont_TerminalLarge();

    int totalSec = (int)std::max(0.0f, gs.countdownTimer);

    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "!%d!", totalSec);

    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 textSize = ImGui::CalcTextSize(timeBuf);
    float cx = gameW * 0.5f - textSize.x * 0.5f;
    float cy = 14.0f + textSize.y * 2.0f;

    // Red pulse — game mechanic, kept as literal
    float pulse = (sinf(globalTime * 6.0f) + 1.0f) * 0.5f;
    uint8_t r = (uint8_t)(230 + pulse * 25);
    ImU32 textCol   = IM_COL32(r, 60, 60, 255);
    ImU32 shadowCol = IM_COL32(80, 0, 0, 180);

    // Stroke simulation (8-direction offset 1px)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            draw->AddText(ImVec2(cx + dx, cy + dy), shadowCol, timeBuf);
        }
    }
    // Main text
    draw->AddText(ImVec2(cx, cy), textCol, timeBuf);
    if (titleFont) ImGui::PopFont();
}

void Crosshair(ImDrawList* draw, float displayW, float displayH) {
    float cx = displayW * 0.5f;
    float cy = displayH * 0.5f;

    constexpr float kGap  = 4.0f;   // gap from center
    constexpr float kLen  = 10.0f;  // line length
    constexpr float kThick = 1.5f;
    ImU32 col = Col32_Accent(160);

    // 4 crosshair segments
    draw->AddLine(ImVec2(cx - kGap - kLen, cy), ImVec2(cx - kGap, cy), col, kThick);  // left
    draw->AddLine(ImVec2(cx + kGap, cy), ImVec2(cx + kGap + kLen, cy), col, kThick);  // right
    draw->AddLine(ImVec2(cx, cy - kGap - kLen), ImVec2(cx, cy - kGap), col, kThick);  // top
    draw->AddLine(ImVec2(cx, cy + kGap), ImVec2(cx, cy + kGap + kLen), col, kThick);  // bottom

    // Center dot
    draw->AddCircleFilled(ImVec2(cx, cy), 1.5f, col, 8);
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
