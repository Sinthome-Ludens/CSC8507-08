/**
 * @file UI_Interaction.h
 * @brief 交互提示渲染（靠近可交互实体时显示 [E] 标签）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderInteractionPrompts(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
