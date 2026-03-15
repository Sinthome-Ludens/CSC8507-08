/**
 * @file UI_MissionSelect.h
 * @brief 关卡选择界面渲染（关卡占位 + 道具/武器选择 + DEPLOY）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderMissionSelect(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
