/**
 * @file UI_Toast.h
 * @brief Toast 浮动通知渲染 + PushToast 工具函数
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"
#include "Game/Components/Res_ToastState.h"

namespace ECS::UI {

void PushToast(Registry& registry, const char* text, ToastType type = ToastType::Info, float duration = 3.0f);
void RenderToasts(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
