/**
 * @brief 玩家 CQC（近战制服）系统实现：状态机推进、拟态激活、背面扇形目标检测。
 *
 * @details
 * - OnUpdate：驱动 Approach → Execute → Complete 三阶段状态机；
 *             F 键优先检测拟态（对休眠敌人），其次检测 CQC 目标（背面扇形）；
 *             Complete 阶段直接对目标挂载死亡组件并推送动作通知；
 *             通过 EntityID 语义的 Sys_Physics 接口冻结目标速度
 */
#include "Sys_PlayerCQC.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_Dying.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Events/Evt_CQC_Takedown.h"
#include "Game/Events/Evt_CQC_Mimicry.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UI_ActionNotify.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"

#include <cmath>
#include <cfloat>

using namespace NCL::Maths;

namespace ECS {

/**
 * @brief 每帧更新 CQC 状态机：冷却计时、阶段推进、目标检测与击杀通知。
 * @details 推进 Approach → Execute → Complete 三阶段状态机，处理休眠敌人拟态和
 *          背后处决目标选择，并在 Complete 阶段通过 Sys_Physics 的 EntityID 接口清零目标速度。
 * @param registry ECS 注册表
 * @param dt       帧时间（秒）
 */
void Sys_PlayerCQC::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_CQCConfig>()) return;
    const auto& config = registry.ctx<Res_CQCConfig>();

    // 获取 EventBus（可选，用于发布事件）
    EventBus* bus = nullptr;
    if (registry.has_ctx<EventBus*>()) {
        bus = registry.ctx<EventBus*>();
    }

    registry.view<C_T_Player, C_D_Input, C_D_Transform, C_D_PlayerState, C_D_CQCState>().each(
        [&](EntityID playerId, C_T_Player&, C_D_Input& input,
            C_D_Transform& playerTf, C_D_PlayerState& ps, C_D_CQCState& cqc) {

            // ── 冷却倒计时 ──
            if (cqc.cooldown > 0.0f) {
                cqc.cooldown -= dt;
                if (cqc.cooldown < 0.0f) cqc.cooldown = 0.0f;
            }

            // ══════════════════════════════════════════════
            // 状态机推进
            // ══════════════════════════════════════════════
            switch (cqc.phase) {
                case CQCPhase::Approach: {
                    cqc.phaseTimer -= dt;
                    if (cqc.phaseTimer <= 0.0f) {
                        cqc.phase = CQCPhase::Execute;
                        cqc.phaseTimer = config.executeTime;
                        LOG_INFO("[CQC] Player " << (int)playerId << " -> Execute");
                    }
                    return; // 阶段进行中，跳过输入检测
                }
                case CQCPhase::Execute: {
                    cqc.phaseTimer -= dt;
                    if (cqc.phaseTimer <= 0.0f) {
                        cqc.phase = CQCPhase::Complete;
                        LOG_INFO("[CQC] Player " << (int)playerId << " -> Complete");
                    }
                    return;
                }
                case CQCPhase::Complete: {
                    EntityID target = cqc.targetEnemy;
                    if (target != 0 && !registry.Has<C_D_Dying>(target)) {
                        // 直接触发死亡动画（无需 C_D_Health）
                        registry.Emplace<C_D_Dying>(target);
                        registry.Emplace<C_D_DeathVisual>(target);
                        LOG_INFO("[Sys_PlayerCQC] CQC kill: entity " << (int)target);

                        // 击杀通知
#ifdef USE_IMGUI
                        ECS::UI::PushActionNotify(registry, "消灭", "敌人", 10,
                                                  ECS::ActionNotifyType::Kill);
#endif

                        // 发布 CQC 完成事件
                        if (bus) {
                            Evt_CQC_Takedown evt{};
                            evt.player = playerId;
                            evt.target = target;
                            evt.position = playerTf.position;
                            bus->publish_deferred(evt);
                        }

                        // 冻结目标速度（防止死亡动画中残留 Jolt 速度漂移）
                        if (registry.has_ctx<Sys_Physics*>()) {
                            auto* physics = registry.ctx<Sys_Physics*>();
                            if (physics && registry.Has<C_D_RigidBody>(cqc.targetEnemy)) {
                                auto& erb = registry.Get<C_D_RigidBody>(cqc.targetEnemy);
                                if (erb.body_created) {
                                    physics->SetLinearVelocity(cqc.targetEnemy, 0.0f, 0.0f, 0.0f);
                                }
                            }
                        }
                    }

                    // 重置状态
                    cqc.phase = CQCPhase::None;
                    cqc.cooldown = config.cooldownTime;
                    cqc.targetEnemy = 0;
                    cqc.phaseTimer = 0.0f;
                    return;
                }
                case CQCPhase::None:
                default:
                    break; // 继续处理输入
            }

            // ══════════════════════════════════════════════
            // F 键输入检测（仅在 None 阶段处理）
            // ══════════════════════════════════════════════
            if (!input.cqcJustPressed) return;

            // 前置条件：站立 + 非伪装 + 非冲刺
            if (ps.stance != PlayerStance::Standing) return;
            if (ps.isDisguised) return;
            if (ps.isSprinting) return;

            // ── 拟态检测：对休眠敌人按 F 获得拟态（不受 cooldown 限制） ──
            {
                EntityID bestMimicTarget = 0;
                float bestMimicDist = FLT_MAX;

                registry.view<C_T_Enemy, C_D_Transform, C_D_EnemyDormant>().each(
                    [&](EntityID enemyId, C_T_Enemy&, C_D_Transform& enemyTf, C_D_EnemyDormant& dormant) {
                        if (!dormant.isDormant) return;
                        if (dormant.hasBeenMimicked) return;

                        float dx = playerTf.position.x - enemyTf.position.x;
                        float dz = playerTf.position.z - enemyTf.position.z;
                        float dist = std::sqrt(dx * dx + dz * dz);

                        if (dist < config.mimicryDistance && dist < bestMimicDist) {
                            bestMimicDist = dist;
                            bestMimicTarget = enemyId;
                        }
                    }
                );

                if (bestMimicTarget != 0) {
                    if (!cqc.isMimicking) {
                        // 激活拟态：先验证双方都有 MeshRenderer，再标记和复制
                        if (registry.Has<C_D_MeshRenderer>(playerId) &&
                            registry.Has<C_D_MeshRenderer>(bestMimicTarget)) {
                            auto& dormant = registry.Get<C_D_EnemyDormant>(bestMimicTarget);
                            dormant.hasBeenMimicked = true;

                            auto& playerMR = registry.Get<C_D_MeshRenderer>(playerId);
                            auto& enemyMR  = registry.Get<C_D_MeshRenderer>(bestMimicTarget);

                            cqc.originalMesh = playerMR.meshHandle;
                            cqc.originalMat  = playerMR.materialHandle;
                            cqc.mimicSource  = bestMimicTarget;
                            cqc.isMimicking  = true;

                            playerMR.meshHandle     = enemyMR.meshHandle;
                            playerMR.materialHandle = enemyMR.materialHandle;

                            LOG_INFO("[CQC] Mimicry activated: Player " << (int)playerId
                                     << " mimics Enemy " << (int)bestMimicTarget);

                            if (bus) {
                                Evt_CQC_Mimicry evt{};
                                evt.player = playerId;
                                evt.source = bestMimicTarget;
                                evt.activated = true;
                                bus->publish_deferred(evt);
                            }
                        }
                    }
                    return; // 拟态操作完成，不继续 CQC 检测
                }
            }

            // 已在拟态中不可发起 CQC
            if (cqc.isMimicking) return;

            // cooldown 仅限制新的 CQC takedown（拟态不受此限制）
            if (cqc.cooldown > 0.0f) return;

            // ── CQC 目标检测：背面扇形 ──
            EntityID bestTarget = 0;
            float bestDist = FLT_MAX;

            registry.view<C_T_Enemy, C_D_Transform, C_D_AIState, C_D_EnemyDormant>().each(
                [&](EntityID enemyId, C_T_Enemy&, C_D_Transform& enemyTf,
                    C_D_AIState& aiState, C_D_EnemyDormant& dormant) {
                    // 休眠敌人不可 CQC
                    if (dormant.isDormant) return;
                    // Hunt 状态不可 CQC
                    if (aiState.current_state == EnemyState::Hunt) return;

                    // 距离检测（XZ 平面）
                    float dx = playerTf.position.x - enemyTf.position.x;
                    float dz = playerTf.position.z - enemyTf.position.z;
                    float dist = std::sqrt(dx * dx + dz * dz);

                    if (dist > config.maxDistance) return;
                    if (dist < 0.001f) return; // 防除零

                    // 背面扇形判定：dot(enemyForward_xz, toPlayer_xz)
                    // -Z 是前方（2.5D 约定）
                    Vector3 enemyForward3D = enemyTf.rotation * Vector3(0.0f, 0.0f, -1.0f);
                    float efx = enemyForward3D.x;
                    float efz = enemyForward3D.z;
                    float efLen = std::sqrt(efx * efx + efz * efz);
                    if (efLen < 0.001f) return;
                    efx /= efLen;
                    efz /= efLen;

                    // toPlayer 方向（从敌人到玩家）
                    float tpx = dx / dist;
                    float tpz = dz / dist;

                    float dotVal = efx * tpx + efz * tpz;

                    // 玩家在敌人背后：dotVal < -dorsalDotMin（即从背后接近）
                    if (dotVal >= -config.dorsalDotMin) return;

                    if (dist < bestDist) {
                        bestDist = dist;
                        bestTarget = enemyId;
                    }
                }
            );

            if (bestTarget != 0) {
                cqc.phase = CQCPhase::Approach;
                cqc.phaseTimer = config.approachTime;
                cqc.targetEnemy = bestTarget;
                LOG_INFO("[CQC] Player " << (int)playerId << " -> Approach on Enemy " << (int)bestTarget);
            }
        }
    );
}

} // namespace ECS
