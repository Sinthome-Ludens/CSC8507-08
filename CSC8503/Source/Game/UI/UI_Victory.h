/**
 * @file UI_Victory.h
 * @brief 战役通关画面渲染（总用时 + 地图名 + 返回菜单按钮）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderVictoryScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
