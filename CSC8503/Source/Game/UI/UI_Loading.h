/**
 * @file UI_Loading.h
 * @brief 加载画面渲染（进度动画 + 系统消息轮播）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderLoadingScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
