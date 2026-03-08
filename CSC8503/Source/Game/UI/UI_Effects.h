/**
 * @file UI_Effects.h
 * @brief 全屏后处理效果：扫描线、暗角、场景过渡（FadeIn/FadeOut）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderScanlineOverlay(float globalTime);
void RenderVignetteOverlay();
void RenderTransitionOverlay(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
