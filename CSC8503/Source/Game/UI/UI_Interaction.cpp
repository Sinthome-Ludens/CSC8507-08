#include "UI_Interaction.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/C_D_Interactable.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// RenderInteractionPrompts — World-space floating labels
// ============================================================

void RenderInteractionPrompts(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    // In a full implementation, we'd iterate entities with C_D_Interactable
    // and project their world positions to screen space.
    // For now this is a placeholder that renders nothing unless
    // entities with C_D_Interactable + screen positions are provided.

    // Example of how a single prompt would render (disabled, for reference):
    // The actual world→screen projection depends on camera matrices which
    // are not available in this UI-only module.

    /*
    float screenX = 400.0f;
    float screenY = 300.0f;
    const char* label = "[E] INTERACT";

    constexpr float kPadX = 10.0f;
    constexpr float kPadY = 6.0f;

    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 labelSize = ImGui::CalcTextSize(label);

    ImVec2 bgMin(screenX - labelSize.x * 0.5f - kPadX,
                 screenY - labelSize.y - kPadY * 2);
    ImVec2 bgMax(screenX + labelSize.x * 0.5f + kPadX,
                 screenY);

    draw->AddRectFilled(bgMin, bgMax,
        IM_COL32(245, 238, 232, 200), 3.0f);
    draw->AddRect(bgMin, bgMax,
        IM_COL32(252, 111, 41, 150), 3.0f, 0, 1.0f);

    draw->AddText(
        ImVec2(screenX - labelSize.x * 0.5f, bgMin.y + kPadY),
        IM_COL32(16, 13, 10, 240), label);

    if (smallFont) ImGui::PopFont();
    */

    (void)draw;
    (void)termFont;
    (void)smallFont;
}

} // namespace ECS::UI

#endif // USE_IMGUI
