/**
 * @file C_D_OrbitTriangle.h
 * @brief 环绕三角形数据组件：标记正在环绕玩家的三角形实体。
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"

namespace ECS {

struct C_D_OrbitTriangle {
    EntityID ownerPlayer  = Entity::NULL_ENTITY; ///< 所属玩家实体
    NCL::Maths::Vector3 targetLocalOffset{0, 0, 0}; ///< 相对玩家的目标偏移
    NCL::Maths::Vector3 currentVelocity{0, 0, 0};   ///< 当前弹簧阻尼速度
    float phaseOffset = 0.0f;  ///< 圆周分布相位偏移（rad）
    int   slotIndex   = 0;     ///< 在 C_D_OrbitInventory 中的槽位索引
};

} // namespace ECS
