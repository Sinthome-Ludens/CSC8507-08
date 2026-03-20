/**
 * @file C_T_OrbOfEnemy.h
 * @brief 标签组件：标识此实体为敌人 Orb（跟随特定敌人实体的位置）。
 *
 * @details
 * 挂载此标签的实体由 `Sys_Spin` 每帧跟随 `ownerID` 实体的世界坐标。
 * 配合 `C_D_Spin` 使用：
 *  - 内层球体：`C_D_Spin.speed = 45.0f`（自旋）
 *  - 外层球体：`C_D_Spin.speed = 0.0f`（只跟随位置）
 *
 * `ownerID` 在 `PrefabFactory::CreateEnemyOrbs` 创建时注入，
 * 指向对应的 `C_T_Enemy` 实体。
 *
 * @see Sys_Spin
 * @see C_D_Spin
 * @see PrefabFactory::CreateEnemyOrbs
 */
#pragma once

#include "Core/ECS/EntityID.h"

namespace ECS {

/**
 * @brief 敌人 Orb 标签：此实体跟随指定敌人实体的位置。
 */
struct C_T_OrbOfEnemy {
    EntityID ownerID = Entity::NULL_ENTITY; ///< 跟随的敌人实体 ID
};

} // namespace ECS
