#include "UI_Effects.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// RenderScanlineOverlay — CRT scanline effect
// ============================================================

void RenderScanlineOverlay(float globalTime) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Subtle horizontal scanlines
    constexpr float lineSpacing = 3.0f;
    constexpr uint8_t lineAlpha = 8;

    for (float y = 0.0f; y < displaySize.y; y += lineSpacing) {
        draw->AddLine(
            ImVec2(0.0f, y),
            ImVec2(displaySize.x, y),
            IM_COL32(16, 13, 10, lineAlpha), 1.0f);
    }

    // Moving scan bar
    float scanY = fmodf(globalTime * 60.0f, displaySize.y + 80.0f) - 40.0f;
    for (int i = 0; i < 20; ++i) {
        float y = scanY + (float)i * 2.0f;
        float t = 1.0f - fabsf((float)i - 10.0f) / 10.0f;
        uint8_t alpha = (uint8_t)(t * 12.0f);
        draw->AddLine(
            ImVec2(0.0f, y),
            ImVec2(displaySize.x, y),
            IM_COL32(245, 238, 232, alpha), 1.0f);
    }
}

// ============================================================
// RenderTransitionOverlay — Scene transition (fade in/out)
// ============================================================

void RenderTransitionOverlay(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    if (!ui.transitionActive) return;

    ui.transitionTimer += dt;
    if (ui.transitionTimer >= ui.transitionDuration) {
        ui.transitionActive = false;
        ui.transitionTimer  = 0.0f;
        return;
    }

    float progress = ui.transitionTimer / ui.transitionDuration;
    progress = std::clamp(progress, 0.0f, 1.0f);

    float alpha;
    if (ui.transitionType == 0) {
        // FadeIn: black -> transparent
        alpha = 1.0f - progress;
    } else {
        // FadeOut: transparent -> black
        alpha = progress;
    }

    uint8_t a = (uint8_t)(alpha * 255.0f);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    draw->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(displaySize.x, displaySize.y),
        IM_COL32(16, 13, 10, a));
}

} // namespace ECS::UI

#endif // USE_IMGUI
