/**
 * @file UI_Chat.h
 * @brief 聊天面板渲染（消息列表 + 回复选项 + 计时条）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderChatPanel(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
