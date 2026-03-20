/**
 * @file C_D_Spin.h
 * @brief 自旋数据组件：驱动实体绕指定轴持续旋转并跟随目标实体位置。
 *
 * @details
 * 挂载此组件的实体每帧由 `Sys_Spin` 驱动：
 *  - 若 `followPlayer == true`：每帧将 `C_D_Transform.position` 同步为
 *    `C_T_Player` 实体的位置 + `(0, yOffset, 0)`
 *  - 若 `speed != 0`：每帧沿 `axis` 指定的世界空间轴累加旋转增量
 *
 * ## 设计约定
 * - 外层球体（装饰外壳）：`speed = 0.0f`，只跟随位置，不旋转
 * - 内层球体（旋转内核）：`speed = 45.0f`，跟随位置并自旋
 * - 敌人 Orb 使用相同组件，`followPlayer = false`，
 *   由 `C_T_OrbOfEnemy.ownerID` 确定跟随目标
 *
 * @see Sys_Spin
 * @see C_T_OrbOfPlayer
 * @see C_T_OrbOfEnemy
 */
#pragma once

#include "Vector.h"

namespace ECS {

/**
 * @brief 自旋与位置跟随数据组件。
 */
struct C_D_Spin {
    NCL::Maths::Vector3 axis{0.0f, 1.0f, 0.0f}; ///< 世界空间旋转轴；可设为斜轴实现双轴视觉效果
    float speed       = 45.0f;                  ///< 旋转速度（度/秒）。0 = 只跟随位置，不旋转
    float yOffset     = 0.0f;                   ///< 相对目标实体的 Y 轴偏移（米）
    bool  enabled     = true;                   ///< false 时 Sys_Spin 跳过此实体
};

} // namespace ECS
