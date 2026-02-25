#include "UI_Effects.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Game/Components/Res_UIState.h"

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

// ============================================================
// RenderTransitionOverlay — CRT启动/关机过渡动画
// ============================================================

void RenderTransitionOverlay(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    if (!ui.transitionActive) return;

    ui.transitionTimer += dt;
    float t = ui.transitionTimer / ui.transitionDuration;
    if (t > 1.0f) t = 1.0f;

    // fade-out: t从0到1黑幕渐显; fade-in: t从0到1黑幕渐隐
    float blackness;
    if (ui.transitionType == 1) {
        // fade-out: 越来越黑
        blackness = t;
    } else {
        // fade-in: 越来越透明
        blackness = 1.0f - t;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // CRT效果：水平扫描线收缩/展开
    float cx = vpPos.x + vpSize.x * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.5f;

    // 中间亮线高度（关机时从全屏缩到一条线，开机反向）
    float lineH;
    if (ui.transitionType == 1) {
        // 关机：线从全高缩到0
        lineH = vpSize.y * (1.0f - t * t); // 非线性收缩
    } else {
        // 开机：线从0展开到全高
        lineH = vpSize.y * t * t;
    }
    float halfH = lineH * 0.5f;

    // 上方黑幕
    if (cy - halfH > vpPos.y) {
        draw->AddRectFilled(
            vpPos,
            ImVec2(vpPos.x + vpSize.x, cy - halfH),
            IM_COL32(0, 0, 0, 255));
    }

    // 下方黑幕
    if (cy + halfH < vpPos.y + vpSize.y) {
        draw->AddRectFilled(
            ImVec2(vpPos.x, cy + halfH),
            ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
            IM_COL32(0, 0, 0, 255));
    }

    // 中间区域扫描线加重
    uint8_t scanAlpha = (uint8_t)(blackness * 40.0f);
    if (scanAlpha > 5) {
        for (float y = cy - halfH; y < cy + halfH; y += 2.0f) {
            draw->AddLine(
                ImVec2(vpPos.x, y), ImVec2(vpPos.x + vpSize.x, y),
                IM_COL32(0, 0, 0, scanAlpha), 1.0f);
        }
    }

    // 中间亮线（CRT关机特征亮线）
    if (lineH < 10.0f && ui.transitionType == 1) {
        uint8_t lineAlpha = (uint8_t)((1.0f - lineH / 10.0f) * 255.0f);
        draw->AddLine(
            ImVec2(vpPos.x, cy), ImVec2(vpPos.x + vpSize.x, cy),
            IM_COL32(0, 220, 210, lineAlpha), 2.0f);
    }

    // 过渡完成
    if (ui.transitionTimer >= ui.transitionDuration) {
        ui.transitionActive = false;
        ui.transitionTimer = 0.0f;
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
