/**
 * @file UI_Loadout.h
 * @brief 装备选择界面渲染（武器/道具装备槽 + 确认按钮）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderLoadoutScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
