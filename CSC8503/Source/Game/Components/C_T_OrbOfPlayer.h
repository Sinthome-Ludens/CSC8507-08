/**
 * @file C_T_OrbOfPlayer.h
 * @brief 标签组件：标识此实体为玩家 Orb（跟随 C_T_Player 实体的位置）。
 *
 * @details
 * 挂载此标签的实体由 `Sys_Spin` 每帧跟随 `C_T_Player` 实体的世界坐标。
 * 配合 `C_D_Spin` 使用：
 *  - 内层球体：`C_D_Spin.speed = 45.0f`（自旋）
 *  - 外层球体：`C_D_Spin.speed = 0.0f`（只跟随位置）
 *
 * @see Sys_Spin
 * @see C_D_Spin
 */
#pragma once

namespace ECS {

/// @brief 玩家 Orb 标签：此实体跟随玩家位置。
struct C_T_OrbOfPlayer {};

} // namespace ECS
