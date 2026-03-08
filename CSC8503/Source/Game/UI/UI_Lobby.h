/**
 * @file UI_Lobby.h
 * @brief 多人大厅界面渲染（Host/Join 按钮 + IP 输入框）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderLobbyScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
