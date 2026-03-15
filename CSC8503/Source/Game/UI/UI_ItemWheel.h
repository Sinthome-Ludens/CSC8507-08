/**
 * @file UI_ItemWheel.h
 * @brief 道具轮盘渲染（TAB 长按弹出，4 扇区选择）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderItemWheel(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
