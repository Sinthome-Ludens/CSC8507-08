/**
 * @file Sys_PlayerCQC.cpp
 * @brief 玩家 CQC（近战制服）系统实现：目标选择 + 高亮管理 + 状态机推进。
 *
 * @details
 * - None 阶段：扫描范围内候选敌人，滚轮切换选中目标，管理 C_D_CQCHighlight 组件；
 *   F 键发起 CQC 进入 Approach 阶段
 * - Approach → Execute → Complete 三阶段状态机
 * - Complete 阶段：对目标挂载死亡组件并推送动作通知
 * - 通过 EntityID 语义的 Sys_Physics 接口冻结目标速度
 */
#include "Sys_PlayerCQC.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_D_CQCHighlight.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_Dying.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Events/Evt_CQC_Takedown.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ScoreConfig.h"
#include "Game/UI/UI_ActionNotify.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"
#include "Game/Events/Evt_Death.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

using namespace NCL::Maths;

namespace ECS {

/**
 * 候选目标条目（用于排序选择）
 */
struct CQCCandidate {
    EntityID id   = 0;
    float    dist = 0.0f;
};

static constexpr int MAX_CANDIDATES = 8;

/**
 * 清除旧高亮组件并重置 CQC 选择状态
 */
static void ClearHighlight(Registry& registry, C_D_CQCState& cqc) {
    if (cqc.highlightedEnemy != 0) {
        if (registry.Valid(cqc.highlightedEnemy) &&
            registry.Has<C_D_CQCHighlight>(cqc.highlightedEnemy)) {
            registry.Remove<C_D_CQCHighlight>(cqc.highlightedEnemy);
        }
        cqc.highlightedEnemy = 0;
    }
}

void Sys_PlayerCQC::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
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
            // 状态机推进（非 None 阶段）
            // ══════════════════════════════════════════════
            switch (cqc.phase) {
                case CQCPhase::Approach: {
                    ClearHighlight(registry, cqc);
                    cqc.phaseTimer -= dt;
                    if (cqc.phaseTimer <= 0.0f) {
                        cqc.phase = CQCPhase::Execute;
                        cqc.phaseTimer = config.executeTime;
                        LOG_INFO("[CQC] Player " << (int)playerId << " -> Execute");
                    }
                    return;
                }
                case CQCPhase::Execute: {
                    ClearHighlight(registry, cqc);
                    cqc.phaseTimer -= dt;
                    if (cqc.phaseTimer <= 0.0f) {
                        cqc.phase = CQCPhase::Complete;
                        LOG_INFO("[CQC] Player " << (int)playerId << " -> Complete");
                    }
                    return;
                }
                case CQCPhase::Complete: {
                    ClearHighlight(registry, cqc);
                    EntityID target = cqc.targetEnemy;
                    if (target != 0 && !registry.Has<C_D_Dying>(target)) {
                        // 设 hp=0 确保 health 状态一致
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
                                Evt_Death evt{};
                                evt.entity    = target;
                                evt.deathType = DeathType::EnemyHpZero;
                                bus->publish_deferred(evt);
                            }
                        }
                        LOG_INFO("[Sys_PlayerCQC] CQC kill: entity " << (int)target);

                        // CQC 击杀扣分（直接写入，不依赖通知系统）
                        if (registry.has_ctx<Res_UIState>()) {
                            Res_ScoreConfig defaultScoreCfg;
                            const auto& sc = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg;
                            auto& uiS = registry.ctx<Res_UIState>();
                            uiS.campaignScore = std::max(0, uiS.campaignScore - sc.penaltyKill);
                            uiS.scoreLost_kills += sc.penaltyKill;
                            uiS.scoreKillCount++;
#ifdef USE_IMGUI
                            ECS::UI::PushActionNotify(registry, "KILL PENALTY", "CQC",
                                                      -sc.penaltyKill, ActionNotifyType::Kill);
#endif
                        }

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
                            if (physics && registry.Has<C_D_RigidBody>(target)) {
                                auto& erb = registry.Get<C_D_RigidBody>(target);
                                if (erb.body_created) {
                                    physics->SetLinearVelocity(target, 0.0f, 0.0f, 0.0f);
                                }
                            }
                        }
                    }

                    // 重置状态
                    cqc.phase = CQCPhase::None;
                    cqc.cooldown = config.cooldownTime;
                    cqc.targetEnemy = 0;
                    cqc.phaseTimer = 0.0f;
                    cqc.selectedIndex = 0;
                    cqc.candidateCount = 0;
                    cqc.highlightedEnemy = 0;
                    return;
                }
                case CQCPhase::None:
                default:
                    break; // 继续处理目标选择
            }

            // ══════════════════════════════════════════════
            // None 阶段：目标选择 + 高亮管理 + F 键发起 CQC
            // ══════════════════════════════════════════════

            // 前置条件：站立 + 非伪装 + 非冲刺
            if (ps.stance != PlayerStance::Standing || ps.isDisguised || ps.isSprinting) {
                ClearHighlight(registry, cqc);
                cqc.selectedIndex = 0;
                cqc.candidateCount = 0;
                return;
            }

            // ── 收集候选目标 ──
            CQCCandidate candidates[MAX_CANDIDATES];
            int count = 0;

            registry.view<C_T_Enemy, C_D_Transform, C_D_AIState, C_D_EnemyDormant>().each(
                [&](EntityID enemyId, C_T_Enemy&, C_D_Transform& enemyTf,
                    C_D_AIState& aiState, C_D_EnemyDormant& dormant) {
                    if (count >= MAX_CANDIDATES) return;
                    // 休眠敌人不可 CQC
                    if (dormant.isDormant) return;
                    // Hunt 状态不可 CQC
                    if (aiState.current_state == EnemyState::Hunt) return;

                    // XZ 平面距离
                    float dx = playerTf.position.x - enemyTf.position.x;
                    float dz = playerTf.position.z - enemyTf.position.z;
                    float dist = std::sqrt(dx * dx + dz * dz);

                    if (dist > config.maxDistance) return;
                    if (dist < 0.001f) return;

                    candidates[count].id   = enemyId;
                    candidates[count].dist = dist;
                    ++count;
                }
            );

            cqc.candidateCount = count;

            // 无候选 → 清除高亮
            if (count == 0) {
                ClearHighlight(registry, cqc);
                cqc.selectedIndex = 0;
                return;
            }

            // ── 按距离升序排序（冒泡，最多 8 个） ──
            for (int i = 0; i < count - 1; ++i) {
                for (int j = 0; j < count - 1 - i; ++j) {
                    if (candidates[j].dist > candidates[j + 1].dist) {
                        CQCCandidate tmp = candidates[j];
                        candidates[j] = candidates[j + 1];
                        candidates[j + 1] = tmp;
                    }
                }
            }

            // ── selectedIndex 稳定性：跟随当前高亮目标 ──
            if (cqc.highlightedEnemy != 0) {
                bool found = false;
                for (int i = 0; i < count; ++i) {
                    if (candidates[i].id == cqc.highlightedEnemy) {
                        cqc.selectedIndex = i;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // 旧高亮不在候选中，clamp
                    if (cqc.selectedIndex >= count) {
                        cqc.selectedIndex = count - 1;
                    }
                }
            } else {
                cqc.selectedIndex = 0;
            }

            // ── 滚轮切换 ──
            if (input.scrollDelta != 0) {
                cqc.selectedIndex += (input.scrollDelta > 0) ? -1 : 1;
                // 循环包裹
                cqc.selectedIndex = ((cqc.selectedIndex % count) + count) % count;
            }

            // ── 高亮管理 ──
            EntityID newHighlight = candidates[cqc.selectedIndex].id;
            if (newHighlight != cqc.highlightedEnemy) {
                // 移除旧高亮
                ClearHighlight(registry, cqc);
                // 挂载新高亮
                if (registry.Valid(newHighlight) && !registry.Has<C_D_CQCHighlight>(newHighlight)) {
                    C_D_CQCHighlight hl;
                    hl.rimColour   = config.highlightRimColour;
                    hl.rimPower    = config.highlightRimPower;
                    hl.rimStrength = config.highlightRimStrength;
                    registry.Emplace<C_D_CQCHighlight>(newHighlight, hl);
                }
                cqc.highlightedEnemy = newHighlight;
            }

            // ── F 键发起 CQC ──
            if (input.cqcJustPressed && cqc.cooldown <= 0.0f && cqc.highlightedEnemy != 0) {
                cqc.phase = CQCPhase::Approach;
                cqc.phaseTimer = config.approachTime;
                cqc.targetEnemy = cqc.highlightedEnemy;

                // 移除高亮（进入 CQC 后不显示）
                ClearHighlight(registry, cqc);
                cqc.selectedIndex = 0;
                cqc.candidateCount = 0;

                LOG_INFO("[CQC] Player " << (int)playerId << " -> Approach on Enemy " << (int)cqc.targetEnemy);
            }
        }
    );
}

} // namespace ECS
