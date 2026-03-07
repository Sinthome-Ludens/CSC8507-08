/**
 * @file UI_Inventory.h
 * @brief 物品栏全屏界面渲染（3x4 网格 + 物品详情面板）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderInventoryScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
