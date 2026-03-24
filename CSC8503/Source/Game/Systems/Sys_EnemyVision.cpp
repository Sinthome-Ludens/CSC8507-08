/**
 * @file Sys_EnemyVision.cpp
 * @brief 敌人视野判定系统实现。
 *
 * 算法概要：
 * - 预收集玩家列表（缓存位置/可见度）
 * - 对每个活跃敌人，依次执行：
 *   1. XZ 距离筛选（超出 maxDistance 跳过）
 *   2. visibilityFactor 门槛（低于 visibilityMin 视为完全隐形）
 *   3. 近距离 360° 感知（closeRange 内无需视锥判定）
 *   4. 扇形视锥检测（XZ 平面点积 vs cosHalfFov）
 *   5. CastRay 遮挡检测（可选，偏移起/终点 Y 坐标，过滤自身/玩家）
 * - 休眠敌人（isDormant）直接标记 is_spotted = false
 */
#include "Sys_EnemyVision.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_D_DDoSFrozen.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Systems/Sys_Physics.h"

#include <cmath>
#include <cfloat>

#ifdef USE_IMGUI
#include "Debug.h"
#include "Game/Components/Res_UIFlags.h"
#endif

using namespace NCL::Maths;

namespace ECS {

static constexpr float PI = 3.14159265358979323846f;

/**
 * @brief 每帧更新所有敌人的视野感知状态。
 *
 * 遍历敌人实体，对每个玩家执行距离/可见度/视锥角判定，
 * 写入 C_D_AIPerception::is_spotted。Shipping 构建中跳过 FOV 线框可视化。
 */
void Sys_EnemyVision::OnUpdate(Registry& registry, float /*dt*/) {
    PAUSE_GUARD(registry);
    if (!registry.has_ctx<Res_VisionConfig>()) return;
    const auto& config = registry.ctx<Res_VisionConfig>();

    // 物理系统指针（遮挡射线用，可选）
    Sys_Physics* physics = nullptr;
    if (config.enableOcclusion && registry.has_ctx<Sys_Physics*>()) {
        physics = registry.ctx<Sys_Physics*>();
    }

    // 预计算半角余弦阈值
    const float halfAngleRad = (config.fovDegrees * 0.5f) * (PI / 180.0f);
    const float cosHalfFov = std::cos(halfAngleRad);

    (void)config.closeRange;  // 近距离感知已移除，仅保留正向视锥

    // ── 1. 预收集玩家列表（避免嵌套 view） ──
    struct PlayerEntry {
        EntityID id;
        Vector3  position;
        float    visibilityFactor;
    };

    PlayerEntry players[4];  // 最多 4 玩家（网络模式）
    int playerCount = 0;

    registry.view<C_T_Player, C_D_Transform, C_D_PlayerState>().each(
        [&](EntityID playerId, C_T_Player&, C_D_Transform& playerTf,
            C_D_PlayerState& ps) {
            if (playerCount >= 4) return;

            players[playerCount].id               = playerId;
            players[playerCount].position          = playerTf.position;
            players[playerCount].visibilityFactor  = ps.visibilityFactor;
            ++playerCount;
        }
    );

    if (playerCount == 0) return;

    // ── 2. 遍历所有敌人，计算 is_spotted ──
    registry.view<C_T_Enemy, C_D_Transform, C_D_AIPerception>().each(
        [&](EntityID enemyId, C_T_Enemy&, C_D_Transform& enemyTf,
            C_D_AIPerception& perception) {

            // 休眠敌人跳过
            if (registry.Has<C_D_EnemyDormant>(enemyId)) {
                const auto& dormant = registry.Get<C_D_EnemyDormant>(enemyId);
                if (dormant.isDormant) {
                    perception.is_spotted = false;
                    return;
                }
            }

            // DDoS 冻结：视野关闭，不感知玩家
            if (registry.Has<C_D_DDoSFrozen>(enemyId)) {
                perception.is_spotted = false;
                return;
            }

            // 计算敌人前向量（XZ 平面）
            Vector3 enemyFwd3D = enemyTf.rotation * Vector3(0.0f, 0.0f, -1.0f);
            float efx = enemyFwd3D.x;
            float efz = enemyFwd3D.z;
            float efLen = std::sqrt(efx * efx + efz * efz);
            bool hasFwd = (efLen > 0.001f);
            if (hasFwd) {
                efx /= efLen;
                efz /= efLen;
            }

            bool spotted = false;
            float spottedDistXZ = config.maxDistance;

            for (int i = 0; i < playerCount; ++i) {
                const auto& player = players[i];

                float dx = player.position.x - enemyTf.position.x;
                float dz = player.position.z - enemyTf.position.z;
                float distXZ = std::sqrt(dx * dx + dz * dz);

                if (distXZ > config.maxDistance) continue;

                if (player.visibilityFactor < config.visibilityMin) continue;

                if (distXZ < 0.001f) {
                    spotted = true;
                    spottedDistXZ = 0.0f;
                } else if (hasFwd) {
                    float tpx = dx / distXZ;
                    float tpz = dz / distXZ;
                    float dotVal = efx * tpx + efz * tpz;

                    if (dotVal < cosHalfFov) continue;
                    spotted = true;
                }

                if (spotted && physics) {
                    float rayOx = enemyTf.position.x;
                    float rayOy = enemyTf.position.y + config.rayOriginHeight;
                    float rayOz = enemyTf.position.z;

                    float rayTx = player.position.x;
                    float rayTy = player.position.y + config.rayTargetHeight;
                    float rayTz = player.position.z;

                    float rayDx = rayTx - rayOx;
                    float rayDy = rayTy - rayOy;
                    float rayDz = rayTz - rayOz;
                    float rayDist = std::sqrt(rayDx * rayDx + rayDy * rayDy + rayDz * rayDz);

                    if (rayDist > 0.001f) {
                        auto hit = physics->CastRayIgnoring(
                            rayOx, rayOy, rayOz,
                            rayDx, rayDy, rayDz, rayDist,
                            enemyId, player.id);
                        if (hit.hit) {
                            spotted = false;
                        }
                    }
                }

                if (spotted) {
                    spottedDistXZ = distXZ;
                    break;
                }
            }

            perception.is_spotted = spotted;
            perception.spotted_distance = spotted ? spottedDistXZ : 0.0f;

#if defined(USE_IMGUI) && !defined(GAME_SHIPPING)
            // FOV 可视化：仅线框模式下画扇形视锥轮廓
            bool wireframe = false;
            if (registry.has_ctx<Res_UIFlags>()) {
                wireframe = registry.ctx<Res_UIFlags>().wireframeMode;
            }
            if (wireframe && hasFwd) {
                const float drawDist = config.maxDistance;
                const float eyeY = enemyTf.position.y + config.rayOriginHeight;
                Vector3 origin(enemyTf.position.x, eyeY, enemyTf.position.z);

                // 颜色：发现玩家 = 红，否则 = 绿
                Vector4 colour = spotted ? Vector4(1, 0, 0, 1) : Vector4(0, 1, 0, 1);

                // 扇形边缘线（左右各 halfAngle）
                float cosHA = std::cos(halfAngleRad);
                float sinHA = std::sin(halfAngleRad);

                // 左边界：旋转 forward 逆时针 halfAngle
                Vector3 leftDir(efx * cosHA - efz * sinHA, 0, efz * cosHA + efx * sinHA);
                // 右边界：旋转 forward 顺时针 halfAngle
                Vector3 rightDir(efx * cosHA + efz * sinHA, 0, efz * cosHA - efx * sinHA);

                Vector3 leftEnd  = origin + leftDir  * drawDist;
                Vector3 rightEnd = origin + rightDir * drawDist;
                Vector3 fwdEnd   = origin + Vector3(efx, 0, efz) * drawDist;

                NCL::Debug::DrawLine(origin, leftEnd,  colour);
                NCL::Debug::DrawLine(origin, rightEnd, colour);
                NCL::Debug::DrawLine(origin, fwdEnd,   colour);
                NCL::Debug::DrawLine(leftEnd, rightEnd, colour);
            }
#endif
        }
    );
}

} // namespace ECS
