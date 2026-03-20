/**
 * @file OrbitTriangleHelper.cpp
 * @brief 环绕三角形双向同步实现。
 */
#include "OrbitTriangleHelper.h"

#include "Game/Components/C_D_OrbitTriangle.h"
#include "Game/Components/C_D_OrbitInventory.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/Res_OrbitConfig.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"

#include <algorithm>
#include <cmath>

namespace ECS {

static constexpr float kPI = 3.14159265358979323846f;

void EnsureOrbitTriangleCount(Registry& reg, EntityID playerId) {
    if (!reg.has_ctx<Res_ItemInventory2>()) return;
    if (!reg.has_ctx<Res_OrbitConfig>()) return;
    if (!reg.Valid(playerId)) return;

    auto& inv    = reg.ctx<Res_ItemInventory2>();
    auto& config = reg.ctx<Res_OrbitConfig>();

    MeshHandle mesh = config.meshHandle;
    if (mesh == 0) {
        LOG_WARN("[OrbitTriangleHelper] meshHandle is 0, skipping triangle creation");
        return;
    }
    int targetCount = static_cast<int>(inv.Get(ItemID::TargetStrike).carriedCount);

    if (!reg.Has<C_D_OrbitInventory>(playerId))
        reg.Emplace<C_D_OrbitInventory>(playerId);
    auto& orbit = reg.Get<C_D_OrbitInventory>(playerId);
    orbit.maxCount = static_cast<int>(inv.Get(ItemID::TargetStrike).maxCarry);

    // 1. 清理无效 EntityID（compact）
    for (int i = 0; i < orbit.count; ) {
        if (!reg.Valid(orbit.triangles[i]) ||
            !reg.Has<C_D_OrbitTriangle>(orbit.triangles[i])) {
            orbit.triangles[i] = orbit.triangles[orbit.count - 1];
            orbit.triangles[orbit.count - 1] = Entity::NULL_ENTITY;
            orbit.count--;
        } else {
            i++;
        }
    }

    // 2. 裁掉多余
    while (orbit.count > targetCount) {
        EntityID excess = orbit.triangles[orbit.count - 1];
        orbit.triangles[orbit.count - 1] = Entity::NULL_ENTITY;
        orbit.count--;
        if (reg.Valid(excess)) reg.Destroy(excess);
    }

    // 3. 补缺
    if (!reg.Has<C_D_Transform>(playerId)) return;
    auto& playerTf = reg.Get<C_D_Transform>(playerId);
    float radius   = config.orbitRadius;
    float height   = config.orbitHeight;

    int spawnCount = std::min(targetCount, C_D_OrbitInventory::kMaxTriangles);
    while (orbit.count < targetCount &&
           orbit.count < C_D_OrbitInventory::kMaxTriangles) {
        int slot = orbit.count;
        int divisor = std::max(1, spawnCount);
        float angle = slot * (2.0f * kPI / divisor);
        NCL::Maths::Vector3 offset(
            std::cos(angle) * radius,
            height,
            std::sin(angle) * radius
        );
        NCL::Maths::Vector3 spawnPos = playerTf.position + offset;

        EntityID tri = PrefabFactory::CreateOrbitTriangle(reg, mesh, slot, spawnPos);

        auto& ot = reg.Emplace<C_D_OrbitTriangle>(tri);
        ot.ownerPlayer     = playerId;
        ot.slotIndex       = slot;
        ot.targetLocalOffset = offset;
        ot.phaseOffset     = angle;

        orbit.triangles[orbit.count++] = tri;

        LOG_INFO("[OrbitTriangleHelper] Spawned orbit triangle slot=" << slot
                 << " entity=" << tri);
    }
}

} // namespace ECS
