/**
 * @file C_D_TriangleProjectile.h
 * @brief 三角形弹射体数据组件：标记已发射、正在飞向目标的三角形实体。
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"

namespace ECS {

struct C_D_TriangleProjectile {
    EntityID ownerPlayer   = Entity::NULL_ENTITY; ///< 发射者
    EntityID targetEnemy   = Entity::NULL_ENTITY; ///< 追踪目标
    NCL::Maths::Vector3 launchDir{0, 0, 1};      ///< 初始飞行方向（无目标时直线飞）
    float    remainingLife = 5.0f;  ///< 剩余生命（秒），超时自毁
    float    speed         = 30.0f; ///< 飞行速度（m/s）
    float    turnRate      = 8.0f;  ///< 转向速率（rad/s）
};

} // namespace ECS
