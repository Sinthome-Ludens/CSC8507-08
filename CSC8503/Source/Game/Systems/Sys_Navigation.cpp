#include "Sys_Navigation.h"
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_NavAgent.h"
#include "Game/Components/C_T_Pathfinder.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Systems/Sys_Physics.h"

namespace ECS {

// ─────────────────────────────────────────────────────────────────────────────
// 辅助：将实体平滑旋转朝向 targetPos（Caution 与路径跟随共用）
// ─────────────────────────────────────────────────────────────────────────────
static void ApplyRotationToward(C_D_NavAgent& agent, C_D_Transform& tf,
                                C_D_RigidBody& rb, Sys_Physics* physics,
                                const NCL::Maths::Vector3& targetPos, float dt)
{
    NCL::Maths::Vector3 dir = targetPos - tf.position;
    dir.y = 0.0f;
    float distSq = dir.x * dir.x + dir.z * dir.z;
    if (distSq < 0.0001f) return;

    float dist = sqrtf(distSq);
    dir.x /= dist;
    dir.z /= dist;

    float targetYaw = atan2f(-dir.x, -dir.z) * 57.29577f;
    NCL::Maths::Quaternion targetRot =
        NCL::Maths::Quaternion::EulerAnglesToQuaternion(0, targetYaw, 0);
    tf.rotation = NCL::Maths::Quaternion::Slerp(
        tf.rotation, targetRot, agent.rotationSpeed * dt);

    if (physics && rb.body_created) {
        physics->SetRotation(rb.jolt_body_id, tf.rotation);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 辅助：按 pathWaypoints 推进路点并施加速度（Alert / Hunt 共用）
// ─────────────────────────────────────────────────────────────────────────────
static void FollowPath(C_D_NavAgent& agent, C_D_Transform& tf,
                       C_D_RigidBody& rb, Sys_Physics* physics, float dt)
{
    if (!agent.isActive || agent.pathLength == 0) return;

    NCL::Maths::Vector3 targetPoint = agent.pathWaypoints[agent.currentWaypointIndex];
    NCL::Maths::Vector3 dir = targetPoint - tf.position;
    dir.y = 0.0f;

    float distSq = dir.x * dir.x + dir.z * dir.z;
    if (distSq < 0.25f) { // 到达路点阈值 0.5m
        if (agent.currentWaypointIndex < agent.pathLength - 1) {
            agent.currentWaypointIndex++;
        } else {
            // 到达终点
            agent.isActive = false;
            if (physics && rb.body_created) {
                physics->SetLinearVelocity(rb.jolt_body_id, 0.0f, 0.0f, 0.0f);
            }
        }
        return;
    }

    float dist = sqrtf(distSq);
    dir.x /= dist;
    dir.z /= dist;

    if (agent.smoothRotation) {
        float targetYaw = atan2f(-dir.x, -dir.z) * 57.29577f;
        NCL::Maths::Quaternion targetRot =
            NCL::Maths::Quaternion::EulerAnglesToQuaternion(0, targetYaw, 0);
        tf.rotation = NCL::Maths::Quaternion::Slerp(
            tf.rotation, targetRot, agent.rotationSpeed * dt);
        if (physics && rb.body_created) {
            physics->SetRotation(rb.jolt_body_id, tf.rotation);
        }
    }

    if (physics && rb.body_created) {
        physics->SetLinearVelocity(rb.jolt_body_id,
            dir.x * agent.speed, -9.8f * dt, dir.z * agent.speed);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 内部辅助：将 std::vector<Vector3> 路径拷贝进 C_D_NavAgent 固定数组
// ─────────────────────────────────────────────────────────────────────────────
static void CopyPathToAgent(C_D_NavAgent& agent,
                            const std::vector<NCL::Maths::Vector3>& path)
{
    const int count = static_cast<int>(
        std::min(path.size(), static_cast<size_t>(NAV_MAX_WAYPOINTS)));
    for (int i = 0; i < count; ++i) {
        agent.pathWaypoints[i] = path[i];
    }
    agent.pathLength           = count;
    agent.currentWaypointIndex = 0;
    agent.isActive             = (count > 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 主更新
// ─────────────────────────────────────────────────────────────────────────────
void Sys_Navigation::OnUpdate(Registry& registry, float dt) {
    if (!m_Pathfinder) return;

    // 从 Registry context 获取 Sys_Physics 指针（由 Sys_Physics::OnAwake 注册）
    Sys_Physics* physics = nullptr;
    if (registry.has_ctx<Sys_Physics*>()) {
        physics = registry.ctx<Sys_Physics*>();
    }

    auto agents = registry.view<C_T_Pathfinder, C_D_NavAgent, C_D_Transform, C_D_RigidBody>();

    agents.each([&](EntityID entity, C_T_Pathfinder& /*tag*/,
                    C_D_NavAgent& agent, C_D_Transform& tf, C_D_RigidBody& rb)
    {
        // ── Step 0: 读取可选 AI 状态 ─────────────────────────────────────
        auto* aiState = registry.TryGet<C_D_AIState>(entity);

        // ── Step 1: 每帧查找最近 NavTarget 实时位置 ───────────────────────
        NCL::Maths::Vector3 liveTargetPos;
        bool targetFound = false;
        float minDistanceSq = 1e10f;

        auto targets = registry.view<C_T_NavTarget, C_D_Transform>();
        targets.each([&](EntityID /*tEnt*/, C_T_NavTarget& tTag, C_D_Transform& tTf) {
            if (std::strcmp(tTag.targetType, agent.searchTag) == 0) {
                NCL::Maths::Vector3 diff = tTf.position - tf.position;
                float dSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                if (dSq < minDistanceSq) {
                    minDistanceSq = dSq;
                    liveTargetPos = tTf.position;
                    targetFound   = true;
                }
            }
        });

        // 找到目标时持续更新最后已知位置
        if (targetFound) {
            agent.lastKnownTargetPos = liveTargetPos;
            agent.hasLastKnownPos    = true;
        }

        // ── Step 2: 按 AI 状态分支（无 AIState 组件视为 Hunt）─────────────
        EnemyState curState = aiState ? aiState->currentState : EnemyState::Hunt;

        switch (curState) {

        // ── Safe：完全静止 ────────────────────────────────────────────────
        case EnemyState::Safe: {
            if (agent.isActive) {
                agent.pathLength           = 0;
                agent.currentWaypointIndex = 0;
                agent.isActive             = false;
                agent.timer                = agent.updateFrequency;
                if (physics && rb.body_created) {
                    physics->SetLinearVelocity(rb.jolt_body_id, 0.0f, 0.0f, 0.0f);
                }
            }
            break;
        }

        // ── Caution：停止移动，朝向最后已知目标位置旋转 ──────────────────
        case EnemyState::Caution: {
            if (agent.isActive) {
                agent.pathLength           = 0;
                agent.currentWaypointIndex = 0;
                agent.isActive             = false;
                agent.timer                = agent.updateFrequency;
                if (physics && rb.body_created) {
                    physics->SetLinearVelocity(rb.jolt_body_id, 0.0f, 0.0f, 0.0f);
                }
            }
            if (agent.smoothRotation && agent.hasLastKnownPos) {
                ApplyRotationToward(agent, tf, rb, physics,
                                    agent.lastKnownTargetPos, dt);
            }
            break;
        }

        // ── Alert：移动到进入 Alert 时的目标位置快照 ─────────────────────
        case EnemyState::Alert: {
            bool justEntered = (agent.prevState != EnemyState::Alert);
            if (justEntered && agent.hasLastKnownPos) {
                // 首次进入：对当前最后已知位置做快照，立即规划路径
                agent.alertSnapshotPos = agent.lastKnownTargetPos;
                std::vector<NCL::Maths::Vector3> tempPath;
                m_Pathfinder->FindPath(tf.position, agent.alertSnapshotPos, tempPath);
                CopyPathToAgent(agent, tempPath);
                agent.timer = 0.0f;
            }
            FollowPath(agent, tf, rb, physics, dt);
            break;
        }

        // ── Hunt：实时追踪目标，定期重新规划路径 ─────────────────────────
        case EnemyState::Hunt:
        default: {
            if (!targetFound) break;

            agent.timer += dt;
            if (agent.timer >= agent.updateFrequency) {
                std::vector<NCL::Maths::Vector3> tempPath;
                m_Pathfinder->FindPath(tf.position, liveTargetPos, tempPath);
                CopyPathToAgent(agent, tempPath);
                agent.timer = 0.0f;
            }
            FollowPath(agent, tf, rb, physics, dt);
            break;
        }
        }

        // ── Step 3: 更新 prevState 供下一帧检测状态切换 ──────────────────
        if (aiState) {
            agent.prevState = aiState->currentState;
        }
    });
}

} // namespace ECS
