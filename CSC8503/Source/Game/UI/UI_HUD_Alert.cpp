/**
 * @file UI_HUD_Alert.cpp
 * @brief HUD sub-module: alert gauge (4-segment colored bar) + score/rating display.
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

void AlertGauge(ImDrawList* draw, const Res_GameState& gs, float gameW) {
    // Hide when countdown is active
    if (gs.countdownActive) return;

    ImFont* termFont = GetFont_Terminal();

    float gaugeW = 220.0f;
    float gaugeH = 14.0f;
    float gaugeX = gameW - gaugeW - 20.0f;
    float gaugeY = 16.0f;
    float alertMax = (gs.alertMax > 0.001f) ? gs.alertMax : 1.0f;

    AlertStatus status = GetAlertStatus(gs.alertLevel);

    // Colored number — game mechanic colors
    ImU32 numCol;
    switch (status) {
        case AlertStatus::Safe:   numCol = IM_COL32(80, 200, 120, 220);  break;
        case AlertStatus::Search: numCol = IM_COL32(220, 200, 0, 220);   break;
        case AlertStatus::Alert:  numCol = IM_COL32(252, 111, 41, 220);  break;
        case AlertStatus::Hunt:   numCol = IM_COL32(220, 60, 40, 220);   break;
        default:                  numCol = Col32_BgDark(220);             break;
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
        Col32_BgDark(60), 2.0f);

    // 4-segment thresholds and colors (game mechanic)
    struct Segment { float threshold; ImU32 color; };
    Segment segments[] = {
        { 25.0f,  IM_COL32(80, 200, 120, 220) },
        { 50.0f,  IM_COL32(220, 200, 0, 220) },
        { 75.0f,  IM_COL32(252, 111, 41, 220) },
        { 100.0f, IM_COL32(220, 60, 40, 220) },
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
        Col32_Gray(100), 2.0f);
}

void Score(ImDrawList* draw, int32_t score, float gameW) {
    ImFont* termFont = GetFont_Terminal();

    const char* rating = GetScoreRating(score);
    int8_t tier = GetScoreRatingTier(score);
    const ImU32 scoreCol = GetScoreRatingColor(tier, 220);

    if (termFont) ImGui::PushFont(termFont);

    char scoreBuf[24];
    snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %d", std::max(0, score));
    ImVec2 scoreSize = ImGui::CalcTextSize(scoreBuf);

    char ratingBuf[8];
    snprintf(ratingBuf, sizeof(ratingBuf), "[%s]", rating);
    ImVec2 ratingSize = ImGui::CalcTextSize(ratingBuf);

    float rightEdge = gameW - 20.0f;
    float scoreY = 60.0f;

    float totalW = scoreSize.x + 8.0f + ratingSize.x;
    float startX = rightEdge - totalW;

    draw->AddText(ImVec2(startX, scoreY), scoreCol, scoreBuf);
    draw->AddText(ImVec2(startX + scoreSize.x + 8.0f, scoreY), scoreCol, ratingBuf);

    if (termFont) ImGui::PopFont();
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
