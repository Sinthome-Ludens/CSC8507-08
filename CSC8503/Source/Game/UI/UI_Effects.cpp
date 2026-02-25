#include "UI_Effects.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>

namespace ECS::UI {

// ============================================================
// RenderScanlineOverlay — CRT扫描线效果
// ============================================================

void RenderScanlineOverlay(float globalTime) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 水平扫描线
    constexpr float lineSpacing = 3.0f;
    float scrollOffset = fmodf(globalTime * 15.0f, lineSpacing);
    ImU32 scanColor = IM_COL32(0, 0, 0, 18);

    for (float y = vpPos.y + scrollOffset; y < vpPos.y + vpSize.y; y += lineSpacing) {
        draw->AddLine(ImVec2(vpPos.x, y), ImVec2(vpPos.x + vpSize.x, y),
            scanColor, 1.0f);
    }

    // 微弱暗角效果
    constexpr float vignetteSize = 200.0f;
    ImU32 vignetteColor = IM_COL32(0, 0, 0, 30);

    // 左上角
    draw->AddRectFilledMultiColor(
        vpPos,
        ImVec2(vpPos.x + vignetteSize, vpPos.y + vignetteSize),
        vignetteColor, IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    // 右上角
    draw->AddRectFilledMultiColor(
        ImVec2(vpPos.x + vpSize.x - vignetteSize, vpPos.y),
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vignetteSize),
        IM_COL32(0, 0, 0, 0), vignetteColor,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    // 左下角
    draw->AddRectFilledMultiColor(
        ImVec2(vpPos.x, vpPos.y + vpSize.y - vignetteSize),
        ImVec2(vpPos.x + vignetteSize, vpPos.y + vpSize.y),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), vignetteColor);
    // 右下角
    draw->AddRectFilledMultiColor(
        ImVec2(vpPos.x + vpSize.x - vignetteSize, vpPos.y + vpSize.y - vignetteSize),
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        vignetteColor, IM_COL32(0, 0, 0, 0));
}

} // namespace ECS::UI

#endif // USE_IMGUI
