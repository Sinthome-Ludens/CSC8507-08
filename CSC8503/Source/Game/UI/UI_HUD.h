/**
 * @file UI_HUD.h
 * @brief 游戏内 HUD 渲染（生命值/警戒度/倒计时/小地图等）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderHUD(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
