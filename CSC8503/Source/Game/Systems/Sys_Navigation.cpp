/**
 * @file Sys_Navigation.cpp
 * @brief 导航系统实现：3D 速度驱动的状态感知寻路与移动控制。
 *
 * @details
 * 根据 C_D_AIState 当前状态分支执行：Safe 静止、Search 旋转朝向、
 * Alert 前往快照位置、Hunt 实时追踪并定期重规划路径。
 * 通过 EntityID 语义的 Sys_Physics 接口同步速度与旋转写回。
 * FollowPath 使用 3D 速度以支持斜坡/多层地图移动。
 */
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
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Utils/Log.h"

namespace ECS {

/**
 * @brief 将实体平滑旋转朝向 targetPos（Search 与路径跟随共用）。
 * @details 该辅助函数被 Search 朝向修正与路径跟随逻辑共用，并在需要时把旋转同步回物理系统。
 * @param entity    当前实体 ID（传入 Sys_Physics::SetRotation）
 * @param agent     NavAgent 数据组件（提供 rotation_speed）
 * @param tf        实体变换组件（读写 rotation）
 * @param rb        刚体组件（检查 body_created）
 * @param physics   物理系统指针（调用 SetRotation）
 * @param targetPos 目标世界坐标（仅使用 XZ 分量）
 * @param dt        帧时间（秒）
 */
static void ApplyRotationToward(EntityID entity, C_D_NavAgent& agent, C_D_Transform& tf,
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
        tf.rotation, targetRot, std::min(agent.rotation_speed * dt, 1.0f));

    if (physics && rb.body_created) {
        physics->SetRotation(entity, tf.rotation);
    }
}

/**
 * @brief 按路径路点推进导航代理（3D 速度驱动）。
 * @details 在 Alert 与 Hunt 状态下复用该辅助逻辑，负责推进当前路点、同步朝向并向物理系统写入期望速度。
 *          使用 3D 距离计算和 3D 速度以支持斜坡/多层地图移动，Y 分量限幅 ±8 m/s 防止飞天。
 * @param entity 当前实体 ID
 * @param agent 导航代理组件
 * @param tf 当前实体变换
 * @param rb 当前实体刚体
 * @param physics 物理系统指针
 * @param dt 本帧时间步长
 */
static void FollowPath(EntityID entity, C_D_NavAgent& agent, C_D_Transform& tf,
                       C_D_RigidBody& rb, Sys_Physics* physics, float dt)
{
    if (!agent.is_active || agent.path_length == 0) return;

    NCL::Maths::Vector3 targetPoint = agent.path_waypoints[agent.current_waypoint_index];
    NCL::Maths::Vector3 dir = targetPoint - tf.position;
    // 不再清零 dir.y，保留 3D 方向以支持斜坡移动

    float distSq = dir.x*dir.x + dir.y*dir.y + dir.z*dir.z;  // 3D 距离
    bool isLastWaypoint = (agent.current_waypoint_index == agent.path_length - 1);

    // ── 前瞻跳跃：若下一路点比当前路点更近，说明 agent 已越过当前路点 ──────────
    // 原因：agent 以全速运动时可能略微冲过路点，此时当前路点已在身后或侧方，
    // 继续朝它移动会产生向后偏转。直接跳到最近的后续路点（保留终点不跳过）。
    while (!isLastWaypoint) {
        const NCL::Maths::Vector3& nxt =
            agent.path_waypoints[agent.current_waypoint_index + 1];
        float ndx = nxt.x - tf.position.x;
        float ndy = nxt.y - tf.position.y;
        float ndz = nxt.z - tf.position.z;
        float nDistSq = ndx*ndx + ndy*ndy + ndz*ndz;  // 3D 距离
        if (nDistSq < distSq) {
            agent.current_waypoint_index++;
            targetPoint  = nxt;
            dir.x = ndx; dir.y = ndy; dir.z = ndz;
            distSq = nDistSq;
            isLastWaypoint = (agent.current_waypoint_index == agent.path_length - 1);
        } else {
            break;
        }
    }

    // 到达阈值：终点使用 stopping_distance，中间路点 0.36m²（0.6m 半径，3D 空间）
    float arrivalSq = isLastWaypoint
        ? (agent.stopping_distance * agent.stopping_distance)
        : 0.36f;

    if (distSq < arrivalSq) {
        if (!isLastWaypoint) {
            agent.current_waypoint_index++;
            // 清零速度，防止转角处惯性冲进墙体
            if (physics && rb.body_created) {
                physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
            }
        } else {
            // 到达终点
            agent.is_active = false;
            if (physics && rb.body_created) {
                physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
            }
        }
        return;
    }

    float dist = sqrtf(distSq);
    dir.x /= dist;
    dir.y /= dist;  // 3D 归一化，保留 Y 分量
    dir.z /= dist;

    // 旋转：仅当距离足够大（>0.2m）才更新方向，防止抖动导致自转
    if (agent.smooth_rotation && distSq > 0.04f) {
        float targetYaw = atan2f(-dir.x, -dir.z) * 57.29577f;
        NCL::Maths::Quaternion targetRot =
            NCL::Maths::Quaternion::EulerAnglesToQuaternion(0, targetYaw, 0);
        tf.rotation = NCL::Maths::Quaternion::Slerp(
            tf.rotation, targetRot, std::min(agent.rotation_speed * dt, 1.0f));
        if (physics && rb.body_created) {
            physics->SetRotation(entity, tf.rotation);
        }
    }

    // 接近中间路点时降速（防止转角冲过头卡进墙角）
    float speed = agent.speed;
    if (!isLastWaypoint && dist < 1.5f) {
        speed *= std::max(0.4f, dist / 1.5f);
    }

    if (physics && rb.body_created) {
        // 3D 速度，Y 分量限幅防止飞天（±8 m/s 足够爬坡）
        float vy = std::clamp(dir.y * speed, -8.0f, 8.0f);
        physics->SetLinearVelocity(entity,
            dir.x * speed, vy, dir.z * speed);
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
        agent.path_waypoints[i] = path[i];
    }
    agent.path_length              = count;
    agent.current_waypoint_index   = 0;
    agent.is_active                = (count > 0);
}

/**
 * @brief 路径重规划后跳过已在到达半径内的初始路点。
 * @details CopyPathToAgent 将 current_waypoint_index 归零，新路径前几个路点
 *          可能已在 agent 当前位置附近甚至身后。若不跳过，agent 会先转向这些路点，
 *          产生每次重规划后的"抖动转向"。
 * @param agent 导航代理组件
 * @param tf    实体变换组件（读取当前位置）
 */
static void SkipReachedWaypoints(C_D_NavAgent& agent, const C_D_Transform& tf)
{
    while (agent.current_waypoint_index < agent.path_length - 1) {
        const NCL::Maths::Vector3& wp =
            agent.path_waypoints[agent.current_waypoint_index];
        float dx = wp.x - tf.position.x;
        float dy = wp.y - tf.position.y;
        float dz = wp.z - tf.position.z;
        if (dx*dx + dy*dy + dz*dz < 0.36f) {   // 3D 距离，0.6m 半径
            agent.current_waypoint_index++;
        } else {
            break;
        }
    }
}

/**
 * @brief 每帧推进所有具有 C_T_Pathfinder + C_D_NavAgent 实体的导航逻辑。
 * @details 根据 AI 状态在 Safe、Search、Alert、Hunt 分支间切换，必要时重规划路径，
 *          并通过 Sys_Physics 的 EntityID 接口同步速度与旋转。
 * @param registry ECS 注册表，用于访问 C_D_AIState、C_D_Transform 等组件。
 * @param dt       帧时间（秒）。
 */
void Sys_Navigation::OnUpdate(Registry& registry, float dt) {
    if (!m_Pathfinder) {
        LOG_WARN("[Sys_Navigation] Pathfinder is null, skipping update.");
        return;
    }

    // 从 Registry context 获取 Sys_Physics 指针（由 Sys_Physics::OnAwake 注册）
    Sys_Physics* physics = nullptr;
    if (registry.has_ctx<Sys_Physics*>()) {
        physics = registry.ctx<Sys_Physics*>();
    }

    // 预收集所有 NavTarget，避免在 agent 循环内重复 ECS 视图扫描（O(n×m) → O(n+m)）
    struct TargetEntry { const char* type; NCL::Maths::Vector3 pos; };
    std::vector<TargetEntry> allTargets;
    registry.view<C_T_NavTarget, C_D_Transform>().each(
        [&](EntityID, C_T_NavTarget& tTag, C_D_Transform& tTf) {
            allTargets.push_back({ tTag.target_type, tTf.position });
        });

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

        for (const auto& entry : allTargets) {
            if (std::strcmp(entry.type, agent.search_tag) == 0) {
                NCL::Maths::Vector3 diff = entry.pos - tf.position;
                float dSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                if (dSq < minDistanceSq) {
                    minDistanceSq = dSq;
                    liveTargetPos = entry.pos;
                    targetFound   = true;
                }
            }
        }

        // 找到目标时持续更新最后已知位置
        if (targetFound) {
            agent.last_known_target_pos = liveTargetPos;
            agent.has_last_known_pos    = true;
        }

        // ── Step 2: 按 AI 状态分支（无 AIState 组件视为 Hunt）─────────────
        EnemyState curState = aiState ? aiState->current_state : EnemyState::Hunt;

        switch (curState) {

        // ── Safe：巡逻（有 C_D_PatrolRoute）或静止（无）────────────────
        case EnemyState::Safe: {
            auto* patrol = registry.TryGet<C_D_PatrolRoute>(entity);
            if (!patrol || patrol->count < 2) {
                // 无巡逻路线或路点不足：完全静止（向后兼容）
                if (agent.is_active) {
                    agent.path_length              = 0;
                    agent.current_waypoint_index   = 0;
                    agent.is_active                = false;
                    agent.timer                    = agent.update_frequency;
                    if (physics && rb.body_created) {
                        physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
                    }
                }
                break;
            }

            // 从非 Safe 状态回落时，标记需要重新规划巡逻路径
            if (agent.prev_state != EnemyState::Safe) {
                patrol->needs_path = true;
            }

            // 到达当前巡逻路点 → 推进到下一个（循环）
            // FollowPath 到达终点后设置 is_active=false，以此检测完成
            if (!agent.is_active && !patrol->needs_path) {
                patrol->current_index = (patrol->current_index + 1) % patrol->count;
                patrol->needs_path    = true;
            }

            // 需要规划到当前巡逻路点的路径
            if (patrol->needs_path) {
                const NCL::Maths::Vector3& dest = patrol->waypoints[patrol->current_index];
                std::vector<NCL::Maths::Vector3> tempPath;
                if (m_Pathfinder->FindPath(tf.position, dest, tempPath)) {
                    CopyPathToAgent(agent, tempPath);
                    SkipReachedWaypoints(agent, tf);
                    patrol->needs_path = false;  // 成功时才清除，失败下帧重试
                }
            }

            { float saved = agent.speed; agent.speed = agent.patrol_speed;
            FollowPath(entity, agent, tf, rb, physics, dt);
            agent.speed = saved; }
            break;
        }

        // ── Search：停止移动，朝向最后已知目标位置旋转 ──────────────────
        case EnemyState::Search: {
            // 进入 Search 时停止并清路径
            agent.path_length              = 0;
            agent.current_waypoint_index   = 0;
            agent.is_active                = false;
            if (physics && rb.body_created) {
                physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
            }
            if (agent.smooth_rotation && agent.has_last_known_pos) {
                ApplyRotationToward(entity, agent, tf, rb, physics,
                                    agent.last_known_target_pos, dt);
            }
            break;
        }

        // ── Alert：移动到最后已知目标位置 ──────────────────────────────
        case EnemyState::Alert: {
            if (agent.prev_state != EnemyState::Alert && agent.has_last_known_pos) {
                // 首次进入：对当前最后已知位置做快照，立即规划路径
                agent.alert_snapshot_pos = agent.last_known_target_pos;
                std::vector<NCL::Maths::Vector3> tempPath;
                if (m_Pathfinder->FindPath(tf.position, agent.alert_snapshot_pos, tempPath)) {
                    CopyPathToAgent(agent, tempPath);
                    SkipReachedWaypoints(agent, tf);
                }
            }
            { float saved = agent.speed; agent.speed = agent.patrol_speed;
            FollowPath(entity, agent, tf, rb, physics, dt);
            agent.speed = saved; }
            break;
        }

        // ── Hunt：实时追踪目标，定期重新规划路径 ─────────────────────────
        case EnemyState::Hunt:
        default: {
            if (!targetFound) break;

            // 已在停止距离内：停止移动，仅保持朝向目标（防止反复规划导致抖动）
            float stopSq = agent.stopping_distance * agent.stopping_distance;
            if (minDistanceSq < stopSq) {
                agent.path_length            = 0;
                agent.current_waypoint_index = 0;
                agent.is_active              = false;
                if (physics && rb.body_created) {
                    physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
                }
                ApplyRotationToward(entity, agent, tf, rb, physics, liveTargetPos, dt);
                break;
            }

            bool justEntered = (agent.prev_state != EnemyState::Hunt);
            if (justEntered) {
                // 首次进入 Hunt：立即规划路径，不等待计时器
                std::vector<NCL::Maths::Vector3> tempPath;
                if (m_Pathfinder->FindPath(tf.position, liveTargetPos, tempPath)) {
                    CopyPathToAgent(agent, tempPath);
                    SkipReachedWaypoints(agent, tf);
                }
                agent.timer = 0.0f;
            } else {
                agent.timer += dt;
                if (agent.timer >= agent.update_frequency) {
                    std::vector<NCL::Maths::Vector3> tempPath;
                    if (m_Pathfinder->FindPath(tf.position, liveTargetPos, tempPath)) {
                        CopyPathToAgent(agent, tempPath);
                        SkipReachedWaypoints(agent, tf);
                    }
                    agent.timer = 0.0f;
                }
            }
            FollowPath(entity, agent, tf, rb, physics, dt);
            break;
        }
        }

        // ── Step 3: 更新 prev_state 供下一帧检测状态切换 ──────────────────
        if (aiState) {
            agent.prev_state = aiState->current_state;
        }
    });
}

} // namespace ECS
