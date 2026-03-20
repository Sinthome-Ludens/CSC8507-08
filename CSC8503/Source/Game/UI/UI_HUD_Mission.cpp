/**
 * @file UI_HUD_Mission.cpp
 * @brief HUD sub-module: left-top mission panel (mission name + objective text).
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

/// @brief Render top-left mission info panel (mission name + objective text).
///
/// Layout: 300x52 dark panel at (16,12) with drop shadow, accent border,
/// mission name in terminal font, and objective text in small font.
void MissionPanel(ImDrawList* draw, const Res_GameState& gs, float /*gameW*/) {
    ImFont* smallFont = GetFont_Small();
    ImFont* termFont  = GetFont_Terminal();

    float panelX = 16.0f;
    float panelY = 12.0f;
    float panelW = 300.0f;
    float panelH = 52.0f;

    // Drop shadow (2-layer offset)
    draw->AddRectFilled(
        ImVec2(panelX + 3.0f, panelY + 3.0f),
        ImVec2(panelX + panelW + 3.0f, panelY + panelH + 3.0f),
        IM_COL32(0, 0, 0, 40), Layout::kPanelRounding);

    // Dark panel background
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        Col32_BgDark(180), Layout::kPanelRounding);

    // Subtle accent border
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        Col32_Accent(40), Layout::kPanelRounding, 0, 1.0f);

    // Mission name
    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(panelX + 10.0f, panelY + 6.0f),
        Col32_Accent(240), gs.missionName);
    if (termFont) ImGui::PopFont();

    // Objective text
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(panelX + 10.0f, panelY + 28.0f),
        Col32_Bg(200), gs.objectiveText);
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
