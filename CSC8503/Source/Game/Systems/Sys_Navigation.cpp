#include "Sys_Navigation.h"
// 必须包含所有用到的组件头文件，解决 C3878 错误
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_NavAgent.h"
#include "Game/Components/C_T_Pathfinder.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Systems/Sys_Physics.h"
#include <cmath>

namespace ECS {
    void Sys_Navigation::OnUpdate(Registry& registry, float dt) {
        if (!m_Pathfinder) return;

        auto agents = registry.view<C_T_Pathfinder, C_D_NavAgent, C_D_Transform, C_D_RigidBody>();

        agents.each([&](EntityID entity, auto& tag, C_D_NavAgent& agent, C_D_Transform& tf, C_D_RigidBody& rb) {

            // 1. 寻找最近目标 (通用标签)
            NCL::Maths::Vector3 targetPos;
            bool targetFound = false;
            float minDistanceSq = 1e10f;

            auto targets = registry.view<C_T_NavTarget, C_D_Transform>();
            targets.each([&](auto tEnt, C_T_NavTarget& tTag, C_D_Transform& tTf) {
                if (tTag.targetType == agent.searchTag) {
                    NCL::Maths::Vector3 diff = tTf.position - tf.position;
                    float dSq = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
                    if (dSq < minDistanceSq) {
                        minDistanceSq = dSq;
                        targetPos = tTf.position;
                        targetFound = true;
                    }
                }
            });

            if (!targetFound) return;

            // 2. 路径更新 (调用接口)
            agent.timer += dt;
            if (agent.timer >= agent.updateFrequency) {
                agent.currentPath.clear();
                m_Pathfinder->FindPath(tf.position, targetPos, agent.currentPath);
                agent.currentWaypointIndex = 0;
                agent.isActive = !agent.currentPath.empty();
                agent.timer = 0.0f;
            }

            // 3. 物理移动与转向
            if (agent.isActive && !agent.currentPath.empty()) {
                NCL::Maths::Vector3 targetPoint = agent.currentPath[agent.currentWaypointIndex];
                NCL::Maths::Vector3 dir = targetPoint - tf.position;
                dir.y = 0;

                float distSq = dir.x*dir.x + dir.z*dir.z;
                if (distSq < 0.25f) { // 到达路点 (0.5m)
                    if (agent.currentWaypointIndex < (int)agent.currentPath.size() - 1) {
                        agent.currentWaypointIndex++;
                    } else {
                        agent.isActive = false;
                    }
                    return;
                }

                float dist = sqrtf(distSq);
                dir.x /= dist; dir.z /= dist; // 手动 Normalise 规避 NCL 库差异

                // 转向逻辑
                if (agent.smoothRotation) {
                    float targetYaw = atan2(-dir.x, -dir.z) * 57.29577f; // Rad to Deg
                    NCL::Maths::Quaternion targetRot = NCL::Maths::Quaternion::EulerAnglesToQuaternion(0, targetYaw, 0);
                    tf.rotation = NCL::Maths::Quaternion::Slerp(tf.rotation, targetRot, agent.rotationSpeed * dt);

                    auto* physics = registry.ctx<Sys_Physics*>();
                    if (physics && rb.body_created) {
                        physics->SetRotation(rb.jolt_body_id, tf.rotation);
                    }
                }

                // 速度应用
                auto* physics = registry.ctx<Sys_Physics*>();
                if (physics && rb.body_created) {
                    physics->SetLinearVelocity(rb.jolt_body_id, dir.x * agent.speed, -9.8f * dt, dir.z * agent.speed);
                }
            }
        });
    }
}