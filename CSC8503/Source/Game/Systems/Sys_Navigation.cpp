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
#include "Game/Utils/PauseGuard.h"
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

static constexpr float kNavDirThresholdSq   = 0.0001f;  ///< 方向向量最小长度²
static constexpr float kNavRotMinDistSq     = 0.04f;    ///< 旋转更新最小距离²
static constexpr float kNavNormalThreshold  = 0.001f;   ///< 法线归一化阈值
static constexpr float kRadToDeg            = 57.29577f; ///< 180/π

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
    if (distSq < kNavDirThresholdSq) return;

    float dist = sqrtf(distSq);
    dir.x /= dist;
    dir.z /= dist;

    float targetYaw = atan2f(-dir.x, -dir.z) * kRadToDeg;
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

    float distSq = dir.x*dir.x + dir.y*dir.y + dir.z*dir.z;
    float xzCurSq = dir.x*dir.x + dir.z*dir.z;
    bool isLastWaypoint = (agent.current_waypoint_index == agent.path_length - 1);

    // ── 前瞻跳跃：使用 XZ 距离比较，Y 高度差不干扰路点推进 ──────────
    while (!isLastWaypoint) {
        const NCL::Maths::Vector3& nxt =
            agent.path_waypoints[agent.current_waypoint_index + 1];
        float ndx = nxt.x - tf.position.x;
        float ndy = nxt.y - tf.position.y;
        float ndz = nxt.z - tf.position.z;
        float nXZSq = ndx*ndx + ndz*ndz;
        if (nXZSq < xzCurSq) {
            agent.current_waypoint_index++;
            targetPoint  = nxt;
            dir.x = ndx; dir.y = ndy; dir.z = ndz;
            distSq = ndx*ndx + ndy*ndy + ndz*ndz;
            xzCurSq = nXZSq;
            isLastWaypoint = (agent.current_waypoint_index == agent.path_length - 1);
        } else {
            break;
        }
    }

    // 到达阈值：XZ 平面距离判定，Y 高度差不阻止路点推进
    float xzDistSq = dir.x * dir.x + dir.z * dir.z;
    float arrivalSq = isLastWaypoint
        ? (agent.stopping_distance * agent.stopping_distance)
        : agent.waypoint_arrival_sq;

    if (xzDistSq < arrivalSq) {
        if (!isLastWaypoint) {
            agent.current_waypoint_index++;
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
    if (agent.smooth_rotation && distSq > kNavRotMinDistSq) {
        float targetYaw = atan2f(-dir.x, -dir.z) * kRadToDeg;
        NCL::Maths::Quaternion targetRot =
            NCL::Maths::Quaternion::EulerAnglesToQuaternion(0, targetYaw, 0);
        tf.rotation = NCL::Maths::Quaternion::Slerp(
            tf.rotation, targetRot, std::min(agent.rotation_speed * dt, 1.0f));
        if (physics && rb.body_created) {
            physics->SetRotation(entity, tf.rotation);
        }
    }

    /**
     * 前方障碍避让（墙面滑移版）：
     * 在移动方向前方 1.5m 发射水平射线，命中非自身障碍物时
     * 将移动方向投影到墙面切线方向（滑移），避免拐角处
     * "停止-重规划"死循环。仅在完全正对墙面无法滑移时才清零速度。
     * 使用 Sys_Physics::CastRayIgnoring（Jolt），不走 NCLGL。
     */
    if (physics && rb.body_created && dist > agent.obstacle_ray_height) {
        auto fwdHit = physics->CastRayIgnoring(
            tf.position.x, tf.position.y + agent.obstacle_ray_height, tf.position.z,
            dir.x, 0.0f, dir.z,
            agent.obstacle_ray_range, entity);
        if (fwdHit.hit && fwdHit.entity != entity) {
            float nx = fwdHit.normalX;
            float nz = fwdHit.normalZ;
            float nLen = sqrtf(nx * nx + nz * nz);
            if (nLen > kNavNormalThreshold) {
                nx /= nLen;
                nz /= nLen;
                float dot = dir.x * nx + dir.z * nz;
                if (dot < 0.0f) {
                    dir.x -= dot * nx;
                    dir.z -= dot * nz;
                    float newLen = sqrtf(dir.x * dir.x + dir.z * dir.z);
                    if (newLen > kNavNormalThreshold) {
                        dir.x /= newLen;
                        dir.z /= newLen;
                    } else {
                        physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
                        agent.timer = agent.update_frequency;
                        return;
                    }
                }
            }
        }
    }

    float speed = agent.speed;
    if (!isLastWaypoint && agent.corner_decel_range > 0.001f && dist < agent.corner_decel_range) {
        speed *= std::max(agent.corner_decel_floor, dist / agent.corner_decel_range);
    }

    if (physics && rb.body_created) {
        float vy = std::clamp(dir.y * speed, -agent.max_vertical_speed, agent.max_vertical_speed);
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
        float dz = wp.z - tf.position.z;
        if (dx*dx + dz*dz < agent.waypoint_arrival_sq) {
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
    PAUSE_GUARD(registry);
    if (!m_Pathfinder) {
        LOG_WARN("[Sys_Navigation] Pathfinder is null, skipping update.");
        return;
    }

    // 从 Registry context 获取 Sys_Physics 指针（由 Sys_Physics::OnAwake 注册）
    Sys_Physics* physics = nullptr;
    if (registry.has_ctx<Sys_Physics*>()) {
        physics = registry.ctx<Sys_Physics*>();
    }

    m_TargetCache.clear();
    registry.view<C_T_NavTarget, C_D_Transform>().each(
        [&](EntityID, C_T_NavTarget& tTag, C_D_Transform& tTf) {
            m_TargetCache.push_back({ tTag.target_type, tTf.position });
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

        for (const auto& entry : m_TargetCache) {
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
                m_ScratchPath.clear();
                if (m_Pathfinder->FindPath(tf.position, dest, m_ScratchPath)) {
                    CopyPathToAgent(agent, m_ScratchPath);
                    SkipReachedWaypoints(agent, tf);
                    patrol->needs_path = false;  // 成功时才清除，失败下帧重试
                }
            }

            { float saved = agent.speed; agent.speed = agent.patrol_speed;
            FollowPath(entity, agent, tf, rb, physics, dt);
            agent.speed = saved; }
            break;
        }

        /**
         * Search：两阶段行为（借鉴 ai/ RETURN 概念）
         * 阶段一：首次进入 Search 且有最后已知位置 → 寻路到该位置（patrol_speed）
         * 阶段二：到达调查点后 → 停止移动，朝向最后已知位置旋转
         * 离开 Search 时重置 search_arrived 标志
         */
        case EnemyState::Search: {
            if (agent.prev_state != EnemyState::Search) {
                agent.search_arrived = false;
                if (agent.has_last_known_pos) {
                    m_ScratchPath.clear();
                    if (m_Pathfinder->FindPath(tf.position, agent.last_known_target_pos, m_ScratchPath)) {
                        CopyPathToAgent(agent, m_ScratchPath);
                        SkipReachedWaypoints(agent, tf);
                    } else {
                        agent.path_waypoints[0]      = agent.last_known_target_pos;
                        agent.path_length            = 1;
                        agent.current_waypoint_index = 0;
                        agent.is_active              = true;
                    }
                } else {
                    agent.search_arrived = true;
                }
            }

            if (!agent.search_arrived) {
                { float saved = agent.speed; agent.speed = agent.patrol_speed;
                FollowPath(entity, agent, tf, rb, physics, dt);
                agent.speed = saved; }
                if (!agent.is_active) {
                    agent.search_arrived = true;
                }
            } else {
                agent.path_length            = 0;
                agent.current_waypoint_index = 0;
                agent.is_active              = false;
                if (physics && rb.body_created) {
                    physics->SetLinearVelocity(entity, 0.0f, 0.0f, 0.0f);
                }
                if (agent.smooth_rotation && agent.has_last_known_pos) {
                    ApplyRotationToward(entity, agent, tf, rb, physics,
                                        agent.last_known_target_pos, dt);
                }
            }
            break;
        }

        // ── Alert：移动到最后已知目标位置 ──────────────────────────────
        case EnemyState::Alert: {
            if (agent.prev_state != EnemyState::Alert && agent.has_last_known_pos) {
                // 首次进入：对当前最后已知位置做快照，立即规划路径
                agent.alert_snapshot_pos = agent.last_known_target_pos;
                m_ScratchPath.clear();
                if (m_Pathfinder->FindPath(tf.position, agent.alert_snapshot_pos, m_ScratchPath)) {
                    CopyPathToAgent(agent, m_ScratchPath);
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
                m_ScratchPath.clear();
                if (m_Pathfinder->FindPath(tf.position, liveTargetPos, m_ScratchPath)) {
                    CopyPathToAgent(agent, m_ScratchPath);
                    SkipReachedWaypoints(agent, tf);
                } else {
                    agent.path_waypoints[0]      = liveTargetPos;
                    agent.path_length            = 1;
                    agent.current_waypoint_index = 0;
                    agent.is_active              = true;
                }
                agent.timer = 0.0f;
            } else {
                agent.timer += dt;
                if (agent.timer >= agent.update_frequency) {
                    m_ScratchPath.clear();
                    if (m_Pathfinder->FindPath(tf.position, liveTargetPos, m_ScratchPath)) {
                        CopyPathToAgent(agent, m_ScratchPath);
                        SkipReachedWaypoints(agent, tf);
                    } else {
                        agent.path_waypoints[0]      = liveTargetPos;
                        agent.path_length            = 1;
                        agent.current_waypoint_index = 0;
                        agent.is_active              = true;
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
