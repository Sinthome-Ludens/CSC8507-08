/**
 * @file Sys_Spin.cpp
 * @brief Orb 自旋与位置跟随系统实现。
 */
#include "Sys_Spin.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Spin.h"
#include "Game/Components/C_T_OrbOfPlayer.h"
#include "Game/Components/C_T_OrbOfEnemy.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Utils/PauseGuard.h"
#include "Quaternion.h"
#include "Vector.h"

#include <vector>

using namespace NCL::Maths;

namespace ECS {

static Quaternion MakeSpinDelta(const C_D_Spin& spin, float dt) {
    Vector3 axis = spin.axis;
    if (Vector::LengthSquared(axis) <= 0.000001f) {
        axis = Vector3(0.0f, 1.0f, 0.0f);
    } else {
        axis = Vector::Normalise(axis);
    }

    return Quaternion::AxisAngleToQuaterion(axis, spin.speed * dt);
}

void Sys_Spin::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);

    // ── Step 0：驱动通用自旋实体（仅旋转，不跟随目标）──────────────
    registry.view<C_D_Spin, C_D_Transform>().each(
        [&](EntityID id, C_D_Spin& spin, C_D_Transform& tf) {
            if (!spin.enabled) return;
            if (registry.Has<C_T_OrbOfPlayer>(id)) return;
            if (registry.Has<C_T_OrbOfEnemy>(id)) return;
            if (spin.speed == 0.0f) return;

            Quaternion delta = MakeSpinDelta(spin, dt);
            tf.rotation = delta * tf.rotation;
            tf.rotation.Normalise();
        }
    );

    // ── Step 1：查询玩家世界坐标 ──────────────────────────────────
    Vector3 playerPos{0.0f, 0.0f, 0.0f};
    bool foundPlayer = false;

    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID /*id*/, C_T_Player&, C_D_Transform& tf) {
            playerPos   = tf.position;
            foundPlayer = true;
        }
    );

    // ── Step 2：驱动玩家 Orb（跟随玩家 + 可选旋转）──────────────
    if (foundPlayer) {
        registry.view<C_T_OrbOfPlayer, C_D_Spin, C_D_Transform>().each(
            [&](EntityID /*id*/, C_T_OrbOfPlayer&, C_D_Spin& spin, C_D_Transform& tf) {
                if (!spin.enabled) return;

                // 同步位置到玩家坐标 + Y 偏移
                tf.position = playerPos + Vector3(0.0f, spin.yOffset, 0.0f);

                // 若速度不为零则累加旋转
                if (spin.speed != 0.0f) {
                    Quaternion delta = MakeSpinDelta(spin, dt);
                    tf.rotation = delta * tf.rotation;
                    tf.rotation.Normalise();
                }
            }
        );
    }

    // ── Step 3：驱动敌人 Orb（跟随各自对应的敌人 + 可选旋转）───
    // 敌人被销毁后（Sys_DeathEffect 动画结束），Orb 的 ownerID 失效 → 收集并销毁
    std::vector<EntityID> orbsToDestroy;

    registry.view<C_T_OrbOfEnemy, C_D_Spin, C_D_Transform>().each(
        [&](EntityID id, C_T_OrbOfEnemy& tag, C_D_Spin& spin, C_D_Transform& tf) {
            if (!spin.enabled) return;
            if (tag.ownerID == Entity::NULL_ENTITY) { orbsToDestroy.push_back(id); return; }
            if (!registry.Valid(tag.ownerID))        { orbsToDestroy.push_back(id); return; }

            // 读取敌人位置
            auto* ownerTf = registry.TryGet<C_D_Transform>(tag.ownerID);
            if (!ownerTf) { orbsToDestroy.push_back(id); return; }

            // 同步位置到敌人坐标 + Y 偏移
            tf.position = ownerTf->position + Vector3(0.0f, spin.yOffset, 0.0f);

            // 若速度不为零则累加旋转
            if (spin.speed != 0.0f) {
                Quaternion delta = MakeSpinDelta(spin, dt);
                tf.rotation = delta * tf.rotation;
                tf.rotation.Normalise();
            }
        }
    );

    for (EntityID id : orbsToDestroy) {
        if (registry.Valid(id)) registry.Destroy(id);
    }
}

} // namespace ECS
