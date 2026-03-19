/**
 * @file UI_MissionSelect.h
 * @brief 关卡选择界面渲染（道具/武器选择 + DEPLOY）
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

/**
 * @brief 渲染关卡选择界面（两列布局：道具 / 武器）并处理导航输入。
 * @param registry ECS 注册表（读写 Res_UIState：Tab/Cursor/Equipped；
 *                 读 Res_ItemInventory2 或 fallback + savedStoreCount 显示库存）
 * @param dt       帧间隔（当前未使用）
 * @details A/D 切换 Tab，W/S 导航条目，Enter/Space 装备/选择，C 键触发 DEPLOY。
 *          DEPLOY 仅设置 pendingSceneRequest=StartGame，实际装备同步延迟到 Scene_PhysicsTest::OnEnter。
 */
void RenderMissionSelect(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
