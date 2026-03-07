/**
 * @file UI_Team.h
 * @brief 团队/阵营选择界面渲染
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderTeamScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
