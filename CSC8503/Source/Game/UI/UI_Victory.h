/**
 * @file UI_Victory.h
 * @brief 战役通关画面渲染（总用时 + 地图名 + 返回菜单按钮）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

/**
 * @brief 渲染战役通关画面（总用时 + 已通关地图 + 返回菜单按钮）。
 * @param registry ECS Registry（读取 Res_UIState 获取通关数据）
 * @param dt       帧间隔时间（当前未使用）
 */
void RenderVictoryScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
