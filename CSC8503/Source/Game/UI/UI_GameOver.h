/**
 * @file UI_GameOver.h
 * @brief 游戏结束画面渲染（统计信息 + 重开/返回菜单按钮）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderGameOverScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
