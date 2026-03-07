/**
 * @file UI_TitleScreen.h
 * @brief 标题画面渲染（游戏 Logo + "Press Any Key" 动画）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderTitleScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
