/**
 * @file OrbitTriangleHelper.h
 * @brief 环绕三角形数量同步 helper（双向同步：清理无效 → 裁多余 → 补缺）。
 */
#pragma once

#include "Core/ECS/Registry.h"
#include "Core/ECS/EntityID.h"

namespace ECS {

/// 确保玩家的环绕三角形数量 == TargetStrike.carriedCount
void EnsureOrbitTriangleCount(Registry& registry, EntityID playerId);

} // namespace ECS
