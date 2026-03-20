/**
 * @file C_D_OrbitInventory.h
 * @brief 环绕三角形库存组件：挂在玩家实体上，记录当前环绕三角形列表。
 */
#pragma once

#include "Core/ECS/EntityID.h"

namespace ECS {

struct C_D_OrbitInventory {
    static constexpr int kMaxTriangles = 8;
    EntityID triangles[kMaxTriangles] = {}; ///< 环绕三角形实体 ID 数组
    int count    = 0; ///< 当前环绕数量
    int maxCount = 2; ///< 最大数量（从 TargetStrike.maxCarry 初始化）
};

} // namespace ECS
