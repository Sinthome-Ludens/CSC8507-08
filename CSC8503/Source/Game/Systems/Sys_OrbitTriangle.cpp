/**
 * @file Sys_OrbitTriangle.cpp
 * @brief 环绕三角形系统实现：环绕弹簧阻尼 + 弹射制导 + 命中击杀。
 */
#include "Sys_OrbitTriangle.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_OrbitTriangle.h"
#include "Game/Components/C_D_TriangleProjectile.h"
#include "Game/Components/C_D_OrbitInventory.h"
#include "Game/Components/Res_OrbitConfig.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_Dying.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Events/Evt_Death.h"
#include "Game/Utils/OrbitTriangleHelper.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"
#include "Core/Bridge/AssetManager.h"
#include "Assets.h"

#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ScoreConfig.h"
#ifdef USE_IMGUI
#include "Game/UI/UI_ActionNotify.h"
#endif

#include <cmath>
#include <cfloat>
#include <vector>
#include <algorithm>

using namespace NCL::Maths;

namespace ECS {

static constexpr float kPI = 3.14159265358979323846f;

// ============================================================
// OnAwake
// ============================================================
void Sys_OrbitTriangle::OnAwake(Registry& registry) {
    // 初始化 Res_OrbitConfig（若尚未存在）
    if (!registry.has_ctx<Res_OrbitConfig>()) {
        registry.ctx_emplace<Res_OrbitConfig>();
    }

    // 加载三角形 mesh 并缓存到 config
    auto& config = registry.ctx<Res_OrbitConfig>();
    if (config.meshHandle == 0) {
        config.meshHandle = AssetManager::Instance().LoadMesh(
            NCL::Assets::MESHDIR + "OrbitTriangle.gltf");
    }

    m_NeedInitialRebuild = true;
    m_GlobalTime = 0.0f;

    LOG_INFO("[Sys_OrbitTriangle] OnAwake complete.");
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_OrbitTriangle::OnDestroy(Registry& /*registry*/) {
    m_NeedInitialRebuild = true;
    m_GlobalTime = 0.0f;
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_OrbitTriangle::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);

    m_GlobalTime += dt;

    // ── Part 0: 首帧延迟 Rebuild ──
    if (m_NeedInitialRebuild) {
        m_NeedInitialRebuild = false;
        registry.view<C_T_Player>().each(
            [&](EntityID playerId, C_T_Player&) {
                EnsureOrbitTriangleCount(registry, playerId);
            }
        );
    }

    if (!registry.has_ctx<Res_OrbitConfig>()) return;
    auto& config = registry.ctx<Res_OrbitConfig>();

    // ── Part A: Orbit 弹簧阻尼 ──
    {
        // 先收集数据再修改，避免 view 迭代中修改问题
        struct OrbitWork {
            EntityID entity;
            EntityID owner;
            int slotIndex;
            float phaseOffset;
        };
        std::vector<OrbitWork> workList;

        registry.view<C_D_OrbitTriangle, C_D_Transform>().each(
            [&](EntityID eid, C_D_OrbitTriangle& ot, C_D_Transform&) {
                workList.push_back({eid, ot.ownerPlayer, ot.slotIndex, ot.phaseOffset});
            }
        );

        for (auto& w : workList) {
            if (!registry.Valid(w.entity)) continue;
            if (!registry.Valid(w.owner)) continue;
            if (!registry.Has<C_D_Transform>(w.owner)) continue;
            if (!registry.Has<C_D_OrbitTriangle>(w.entity)) continue;

            auto& ot = registry.Get<C_D_OrbitTriangle>(w.entity);
            auto& tf = registry.Get<C_D_Transform>(w.entity);
            auto& ownerTf = registry.Get<C_D_Transform>(w.owner);

            // 目标位置：等角分布圆周 + 公转
            float baseAngle = w.phaseOffset + m_GlobalTime * config.orbitSpeed;
            Vector3 targetPos(
                ownerTf.position.x + std::cos(baseAngle) * config.orbitRadius,
                ownerTf.position.y + config.orbitHeight
                    + std::sin(m_GlobalTime * config.hoverFreq * 2.0f * kPI + w.phaseOffset) * config.hoverAmplitude,
                ownerTf.position.z + std::sin(baseAngle) * config.orbitRadius
            );

            // 弹簧阻尼
            Vector3 displacement = targetPos - tf.position;
            Vector3 springForce = displacement * config.springStiffness;
            Vector3 dampingForce = ot.currentVelocity * (-config.springDamping);
            Vector3 accel = springForce + dampingForce;

            ot.currentVelocity = ot.currentVelocity + accel * dt;

            // Clamp velocity to prevent explosion
            float maxVel = config.maxOrbitVelocity;
            float velLen = std::sqrt(ot.currentVelocity.x * ot.currentVelocity.x
                                   + ot.currentVelocity.y * ot.currentVelocity.y
                                   + ot.currentVelocity.z * ot.currentVelocity.z);
            if (velLen > maxVel) {
                float scale = maxVel / velLen;
                ot.currentVelocity = ot.currentVelocity * scale;
            }

            tf.position = tf.position + ot.currentVelocity * dt;

            // 自转
            float spinAngle = m_GlobalTime * config.spinSpeed + w.phaseOffset;
            tf.rotation = Quaternion::EulerAnglesToQuaternion(0.0f, spinAngle * (180.0f / kPI), 0.0f);
        }
    }

    // ── Part B: Projectile 制导（纯代码驱动，保持 kinematic，直接写 Transform） ──
    {
        std::vector<EntityID> toDestroy;

        registry.view<C_D_TriangleProjectile, C_D_Transform>().each(
            [&](EntityID eid, C_D_TriangleProjectile& proj, C_D_Transform& tf) {
                proj.remainingLife -= dt;
                if (proj.remainingLife <= 0.0f) {
                    toDestroy.push_back(eid);
                    return;
                }

                // 验证目标有效性
                if (Entity::IsValid(proj.targetEnemy) &&
                    (!registry.Valid(proj.targetEnemy) ||
                     registry.Has<C_D_Dying>(proj.targetEnemy))) {
                    proj.targetEnemy = Entity::NULL_ENTITY;
                }

                // 如果目标无效，从弹射体当前位置找最近敌人
                if (!Entity::IsValid(proj.targetEnemy)) {
                    float minDist2 = FLT_MAX;
                    EntityID nearest = Entity::NULL_ENTITY;
                    registry.view<C_T_Enemy, C_D_Transform>().each(
                        [&](EntityID enemy, C_T_Enemy&, C_D_Transform& etf) {
                            if (registry.Has<C_D_Dying>(enemy)) return;
                            float dx = etf.position.x - tf.position.x;
                            float dz = etf.position.z - tf.position.z;
                            float d2 = dx * dx + dz * dz;
                            if (d2 < minDist2) {
                                minDist2 = d2;
                                nearest = enemy;
                            }
                        }
                    );
                    proj.targetEnemy = nearest;
                }

                // 计算飞行方向
                Vector3 moveDir = proj.launchDir;

                if (Entity::IsValid(proj.targetEnemy) &&
                    registry.Valid(proj.targetEnemy) &&
                    registry.Has<C_D_Transform>(proj.targetEnemy)) {
                    auto& targetTf = registry.Get<C_D_Transform>(proj.targetEnemy);
                    Vector3 toTarget = targetTf.position - tf.position;
                    toTarget.y = 0.0f;

                    float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

                    // 命中检测
                    if (dist < config.hitRadius) {
                        EntityID target = proj.targetEnemy;
                        if (!registry.Has<C_D_Dying>(target)) {
                            if (registry.Has<C_D_Health>(target)) {
                                auto& hp = registry.Get<C_D_Health>(target);
                                hp.hp = 0.0f;
                                hp.deathCause = DeathType::EnemyHpZero;
                            }
                            registry.Emplace<C_D_Dying>(target);
                            registry.Emplace<C_D_DeathVisual>(target);
                            if (registry.has_ctx<EventBus*>()) {
                                auto* bus = registry.ctx<EventBus*>();
                                if (bus) {
                                    Evt_Death de{};
                                    de.entity = target;
                                    de.deathType = DeathType::EnemyHpZero;
                                    bus->publish_deferred(de);
                                }
                            }
                            // 击杀扣分（与 RoamAI 一致）
                            if (registry.has_ctx<Res_UIState>()) {
                                Res_ScoreConfig defaultSc;
                                const auto& sc = registry.has_ctx<Res_ScoreConfig>()
                                    ? registry.ctx<Res_ScoreConfig>() : defaultSc;
                                auto& uiS = registry.ctx<Res_UIState>();
                                uiS.campaignScore = std::max(0, uiS.campaignScore - sc.penaltyKill);
                                uiS.scoreLost_kills += sc.penaltyKill;
                                uiS.scoreKillCount++;
#ifdef USE_IMGUI
                                ECS::UI::PushActionNotify(registry, "KILL PENALTY", "TARGET STRIKE",
                                                          -sc.penaltyKill, ActionNotifyType::Kill);
#endif
                            }
                            LOG_INFO("[Sys_OrbitTriangle] Projectile " << eid
                                     << " hit enemy " << target);
                        }
                        toDestroy.push_back(eid);
                        return;
                    }

                    // 制导转向：直接朝目标方向飞
                    if (dist > 0.001f) {
                        moveDir = toTarget * (1.0f / dist);
                        // 更新 launchDir 以便无目标时保持最后方向
                        proj.launchDir = moveDir;
                    }
                }
                // 无目标：沿 launchDir 直线飞行

                // 直接移动位置（纯代码驱动，不走 Jolt）
                float step = proj.speed * dt;
                tf.position.x += moveDir.x * step;
                tf.position.z += moveDir.z * step;

                // 朝向飞行方向
                float dirLen = std::sqrt(moveDir.x*moveDir.x + moveDir.z*moveDir.z);
                if (dirLen > 0.01f) {
                    float yaw = std::atan2(moveDir.x, moveDir.z) * (180.0f / kPI);
                    tf.rotation = Quaternion::EulerAnglesToQuaternion(0.0f, yaw, 0.0f);
                }
            }
        );

        for (EntityID d : toDestroy) {
            if (registry.Valid(d)) registry.Destroy(d);
        }
    }

    // ── Part C: 每帧清理无效 OrbitInventory 引用 ──
    registry.view<C_D_OrbitInventory>().each(
        [&](EntityID, C_D_OrbitInventory& orbit) {
            for (int i = 0; i < orbit.count; ) {
                if (!registry.Valid(orbit.triangles[i]) ||
                    !registry.Has<C_D_OrbitTriangle>(orbit.triangles[i])) {
                    orbit.triangles[i] = orbit.triangles[orbit.count - 1];
                    orbit.triangles[orbit.count - 1] = Entity::NULL_ENTITY;
                    orbit.count--;
                } else {
                    i++;
                }
            }
        }
    );
}

} // namespace ECS
