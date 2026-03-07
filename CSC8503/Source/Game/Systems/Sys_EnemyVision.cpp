#include "Sys_EnemyVision.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Systems/Sys_Physics.h"

#include <cmath>
#include <cfloat>

using namespace NCL::Maths;

namespace ECS {

static constexpr float PI = 3.14159265358979323846f;

void Sys_EnemyVision::OnUpdate(Registry& registry, float /*dt*/) {
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

            // 拟态绕过：isMimicking 时不被视觉检测
            if (registry.Has<C_D_CQCState>(playerId)) {
                const auto& cqc = registry.Get<C_D_CQCState>(playerId);
                if (cqc.isMimicking) return;
            }

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

            for (int i = 0; i < playerCount; ++i) {
                const auto& player = players[i];

                // A. XZ 平面距离
                float dx = player.position.x - enemyTf.position.x;
                float dz = player.position.z - enemyTf.position.z;
                float distXZ = std::sqrt(dx * dx + dz * dz);

                if (distXZ > config.maxDistance) continue;

                // 重叠位置直接判定为发现
                if (distXZ < 0.001f) {
                    spotted = true;
                } else {
                    // B. 可见度门槛
                    if (player.visibilityFactor < config.visibilityMin) continue;

                    // C. 近距离 360° 感知
                    if (distXZ <= config.closeRange) {
                        spotted = true;
                    } else if (hasFwd) {
                        // D. 扇形视锥检测
                        float tpx = dx / distXZ;
                        float tpz = dz / distXZ;
                        float dotVal = efx * tpx + efz * tpz;

                        if (dotVal < cosHalfFov) continue;
                        spotted = true;
                    }
                }

                // E. 遮挡检测（射线偏移 + 过滤自身/玩家）
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
                        auto hit = physics->CastRay(rayOx, rayOy, rayOz,
                                                    rayDx, rayDy, rayDz, rayDist);
                        if (hit.hit && hit.entity != enemyId && hit.entity != player.id) {
                            // 命中第三者（墙壁/障碍物）= 有遮挡
                            spotted = false;
                        }
                    }
                }

                if (spotted) break;
            }

            perception.is_spotted = spotted;
        }
    );
}

} // namespace ECS
