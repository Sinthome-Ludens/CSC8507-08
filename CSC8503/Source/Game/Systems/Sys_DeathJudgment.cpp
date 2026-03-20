/**
 * @file Sys_DeathJudgment.cpp
 * @brief 死亡判定系统实现。
 *
 * 算法概要：
 * - OnAwake: 订阅 Evt_Phys_TriggerEnter，回调中检测 C_T_DeathZone 并将受害者 hp 设为 0
 * - OnUpdate:
 *   1. 更新无敌计时器（invTimer 递减）
 *   2. Hunt 敌人抓捕检测（XZ 平方距离 < captureDistance²）
 *   3. 死亡检查：玩家 hp<=0 触发 GameOver 界面，敌人 hp<=0 挂载 C_D_Dying + C_D_DeathVisual，由 Sys_DeathEffect 执行四阶段动画后延迟销毁
 * - OnDestroy: 取消 EventBus 订阅
 */
#include "Sys_DeathJudgment.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_T_DeathZone.h"
#include "Game/Components/C_D_Dying.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ScoreConfig.h"
#include "Game/Components/Res_AudioConfig.h"
#include "Game/Components/Res_EnemyEnums.h"
#ifdef USE_IMGUI
#include "Game/UI/UI_ActionNotify.h"
#endif
#include "Game/Events/Evt_Phys_Trigger.h"
#include "Game/Events/Evt_Phys_Collision.h"
#include "Game/Events/Evt_Death.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Utils/Log.h"

#include <algorithm>

namespace ECS {

/**
 * @brief 系统初始化：订阅 Evt_Phys_TriggerEnter，检测死亡区域并将受害者 HP 设为 0。
 * @param registry ECS 注册表
 */
void Sys_DeathJudgment::OnAwake(Registry& registry) {
    // 订阅触发器进入事件（死亡区域即死）
    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            m_TriggerSubId = bus->subscribe<Evt_Phys_TriggerEnter>(
                [&registry](const Evt_Phys_TriggerEnter& e) {
                    // 检查触发器实体是否是死亡区域
                    bool isTriggerDeathZone = registry.Has<C_T_DeathZone>(e.entity_trigger);
                    bool isOtherDeathZone   = registry.Has<C_T_DeathZone>(e.entity_other);

                    EntityID deathZone = 0;
                    EntityID victim    = 0;

                    if (isTriggerDeathZone) {
                        deathZone = e.entity_trigger;
                        victim    = e.entity_other;
                    } else if (isOtherDeathZone) {
                        deathZone = e.entity_other;
                        victim    = e.entity_trigger;
                    } else {
                        return; // 不涉及死亡区域
                    }

                    // 将受害者 HP 设为 0，标记死因为触发区即死
                    if (registry.Has<C_D_Health>(victim)) {
                        auto& health = registry.Get<C_D_Health>(victim);
                        if (health.hp > 0.0f) {
                            health.hp = 0.0f;
                            health.deathCause = DeathType::PlayerTriggerDie;
                            LOG_INFO("[DeathJudgment] Entity " << (int)victim
                                     << " entered death zone " << (int)deathZone);
                        }
                    }
                }
            );

            // 订阅刚体碰撞事件（敌人-玩家碰撞即死）
            m_CollisionSubId = bus->subscribe<Evt_Phys_Collision>(
                [&registry](const Evt_Phys_Collision& e) {
                    // 判断碰撞双方哪个是玩家、哪个是敌人
                    EntityID playerId = 0;
                    EntityID enemyId  = 0;

                    bool aIsPlayer = registry.Has<C_T_Player>(e.entity_a);
                    bool bIsPlayer = registry.Has<C_T_Player>(e.entity_b);
                    bool aIsEnemy  = registry.Has<C_T_Enemy>(e.entity_a);
                    bool bIsEnemy  = registry.Has<C_T_Enemy>(e.entity_b);

                    if (aIsPlayer && bIsEnemy) {
                        playerId = e.entity_a;
                        enemyId  = e.entity_b;
                    } else if (bIsPlayer && aIsEnemy) {
                        playerId = e.entity_b;
                        enemyId  = e.entity_a;
                    } else {
                        return; // 不是玩家-敌人碰撞
                    }

                    // 休眠敌人跳过
                    if (registry.Has<C_D_EnemyDormant>(enemyId)) {
                        const auto& dormant = registry.Get<C_D_EnemyDormant>(enemyId);
                        if (dormant.isDormant) return;
                    }

                    // 游戏已结束跳过
                    if (registry.has_ctx<Res_GameState>() &&
                        registry.ctx<Res_GameState>().isGameOver) return;

                    if (!registry.Has<C_D_Health>(playerId)) return;
                    auto& health = registry.Get<C_D_Health>(playerId);

                    // 玩家已死或无敌跳过
                    if (health.hp <= 0.0f) return;
                    if (health.invTimer > 0.0f) return;

                    // CQC 期间：仅免疫正在处决的目标敌人，其他敌人碰撞仍致死
                    if (registry.Has<C_D_CQCState>(playerId)) {
                        const auto& cqc = registry.Get<C_D_CQCState>(playerId);
                        if (cqc.phase != CQCPhase::None && enemyId == cqc.targetEnemy) return;
                    }

                    // 碰撞即死
                    health.hp = 0.0f;
                    health.deathCause = DeathType::PlayerCaptured;
                    LOG_INFO("[DeathJudgment] Player " << (int)playerId
                             << " killed by collision with enemy " << (int)enemyId);
                }
            );
        }
    }

    LOG_INFO("[Sys_DeathJudgment] OnAwake");
}

/**
 * @brief 每帧更新：无敌计时器递减、Hunt 抓捕检测、死亡检查与通知推送。
 * @param registry ECS 注册表
 * @param dt       帧时间（秒）
 */
void Sys_DeathJudgment::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
    if (!registry.has_ctx<Res_DeathConfig>()) return;
    const auto& config = registry.ctx<Res_DeathConfig>();

    // ── 1. 更新无敌计时器 ──
    registry.view<C_D_Health>().each(
        [dt](EntityID, C_D_Health& health) {
            if (health.invTimer > 0.0f) {
                health.invTimer -= dt;
                if (health.invTimer < 0.0f) health.invTimer = 0.0f;
            }
        }
    );

    // ── 2. Hunt 敌人抓捕检测 ──
    // 预收集玩家列表
    struct PlayerEntry {
        EntityID id;
        float    px, pz;
    };
    PlayerEntry players[4];
    int playerCount = 0;

    registry.view<C_T_Player, C_D_Transform, C_D_Health>().each(
        [&](EntityID playerId, C_T_Player&, C_D_Transform& tf, C_D_Health& health) {
            if (playerCount >= 4) return;
            if (health.hp <= 0.0f) return; // 已死亡的玩家不再被抓
            players[playerCount].id = playerId;
            players[playerCount].px = tf.position.x;
            players[playerCount].pz = tf.position.z;
            ++playerCount;
        }
    );

    if (!m_DidLogStartupState) {
        int enemyCount = 0;
        int healthCount = 0;
        registry.view<C_T_Enemy>().each([&](EntityID, C_T_Enemy&) { ++enemyCount; });
        registry.view<C_D_Health>().each([&](EntityID, C_D_Health&) { ++healthCount; });

        int matchPhase = -1;
        bool isMultiplayer = false;
        if (registry.has_ctx<Res_GameState>()) {
            const auto& gs = registry.ctx<Res_GameState>();
            matchPhase = static_cast<int>(gs.matchPhase);
            isMultiplayer = gs.isMultiplayer;
        }

        LOG_MPDBG("[Sys_DeathJudgment] Startup snapshot: players=" << playerCount
                  << " enemies=" << enemyCount
                  << " healthEntities=" << healthCount
                  << " isMultiplayer=" << isMultiplayer
                  << " matchPhase=" << matchPhase);
        m_DidLogStartupState = true;
    }

    const float captureDistSq = config.captureDistance * config.captureDistance;

    if (playerCount > 0) {
        registry.view<C_T_Enemy, C_D_AIState, C_D_Transform>().each(
            [&](EntityID enemyId, C_T_Enemy&, C_D_AIState& state, C_D_Transform& enemyTf) {
                // 休眠敌人跳过
                if (registry.Has<C_D_EnemyDormant>(enemyId)) {
                    const auto& dormant = registry.Get<C_D_EnemyDormant>(enemyId);
                    if (dormant.isDormant) return;
                }

                // Only enemies in Hunt state are allowed to capture players.
                if (state.current_state != EnemyState::Hunt) {
                    return;
                }

                for (int i = 0; i < playerCount; ++i) {
                    float dx = players[i].px - enemyTf.position.x;
                    float dz = players[i].pz - enemyTf.position.z;
                    float distSq = dx * dx + dz * dz;

                    if (distSq < captureDistSq) {
                        // 抓捕成功，标记死因为敌人抓捕
                        if (registry.Has<C_D_Health>(players[i].id)) {
                            auto& health = registry.Get<C_D_Health>(players[i].id);
                            if (health.hp > 0.0f && health.invTimer <= 0.0f) {
                                health.hp = 0.0f;
                                health.deathCause = DeathType::PlayerCaptured;
                                LOG_INFO("[DeathJudgment] Player " << (int)players[i].id
                                         << " captured by enemy " << (int)enemyId);
                            }
                        }
                    }
                }
            }
        );
    }

    // ── 3. 死亡检查 ──
    // 已 GameOver 则跳过，避免每帧重复发布 Evt_Death
    if (registry.has_ctx<Res_GameState>()) {
        const auto& gs = registry.ctx<Res_GameState>();
        if (gs.isGameOver
            || (gs.isMultiplayer
                && (gs.matchPhase != MatchPhase::Running
                    || gs.localTerminalState != MultiplayerTerminalState::None))) {
            return;
        }
    }
    // Registry::Destroy 是延迟销毁（加入 m_PendingDestroy），
    // 帧末 ProcessPendingDestroy 才真正移除实体，不会在 view.each()
    // 迭代期间使迭代器失效，因此可安全在循环内直接调用。
    bool playerDied = false;

    registry.view<C_D_Health>().each(
        [&](EntityID entity, C_D_Health& health) {
            if (health.hp > 0.0f) return;

            if (registry.Has<C_T_Player>(entity)) {
                // 玩家死亡 → GameOver 界面
                if (!playerDied) {
                    playerDied = true;
                    LOG_INFO("[DeathJudgment] Player " << (int)entity << " died, triggering GameOver");

                    if (registry.has_ctx<EventBus*>()) {
                        auto* bus = registry.ctx<EventBus*>();
                        if (bus) {
                            Evt_Death evt;
                            evt.entity    = entity;
                            evt.deathType = health.deathCause;
                            bus->publish_deferred(evt);
                        }
                    }

                    if (registry.has_ctx<Res_GameState>() && registry.has_ctx<Res_UIState>()) {
                        auto& gs = registry.ctx<Res_GameState>();
                        auto& ui = registry.ctx<Res_UIState>();
                        if (gs.isMultiplayer) {
                            gs.localTerminalState = MultiplayerTerminalState::Death;
                            gs.localTerminalReason = ToU8(GameOverReason::Detected);
                        } else {
                            gs.isGameOver       = true;
                            gs.gameOverReason   = GameOverReason::Detected;
                            gs.gameOverTime     = gs.playTime;
                            ui.activeScreen = UIScreen::GameOver;
                            ui.gameOverSelectedIndex = 0;
                        }
                        if (registry.has_ctx<Res_AudioState>()) {
                            auto& audio = registry.ctx<Res_AudioState>();
                            audio.requestedBgm = BgmId::Defeat;
                            audio.bgmOverride  = false;
                        }
                        // 失败惩罚 -500（挑战模式全局规则）
                        Res_ScoreConfig defaultScoreCfg;
                        const auto& scoreCfg = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg;
                        if (!ui.failureScorePenaltyApplied) {
                            ui.failureScorePenaltyApplied = true;
                            ui.scoreLost_failure += scoreCfg.penaltyCapture;
                            ui.campaignScore = std::max(0, ui.campaignScore - scoreCfg.penaltyCapture);
#ifdef USE_IMGUI
                            ECS::UI::PushActionNotify(registry, "MISSION", "FAILED",
                                                      -scoreCfg.penaltyCapture, ActionNotifyType::Alert);
#endif
                        }
                    }
                }
            } else if (registry.Has<C_T_Enemy>(entity)) {
                // 敌人死亡 → 挂载死亡特效组件（由 Sys_DeathEffect 播放动画后销毁）
                if (registry.Has<C_D_Dying>(entity)) return; // 已在播放死亡动画
                LOG_INFO("[DeathJudgment] Enemy " << (int)entity << " died, starting death effect");

                if (registry.has_ctx<EventBus*>()) {
                    auto* bus = registry.ctx<EventBus*>();
                    if (bus) {
                        Evt_Death evt;
                        evt.entity    = entity;
                        evt.deathType = DeathType::EnemyHpZero;
                        bus->publish_deferred(evt);
                    }
                }

                registry.Emplace<C_D_Dying>(entity);
                registry.Emplace<C_D_DeathVisual>(entity);

                // 击杀扣分通知 -10（挑战模式全局规则）
                if (registry.has_ctx<Res_UIState>()
                    && registry.has_ctx<Res_GameState>()) {
                    Res_ScoreConfig defaultScoreCfg2;
                    const auto& sc = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg2;
                    auto& uiS = registry.ctx<Res_UIState>();
                    uiS.campaignScore = std::max(0, uiS.campaignScore - sc.penaltyKill);
                    uiS.scoreLost_kills += sc.penaltyKill;
                    uiS.scoreKillCount++;
#ifdef USE_IMGUI
                    ECS::UI::PushActionNotify(registry, "KILL PENALTY", "ENEMY",
                                              -sc.penaltyKill, ActionNotifyType::Kill);
#endif
                }
            }
        }
    );
}

/**
 * @brief 系统销毁：取消 EventBus 中的 TriggerEnter 订阅。
 * @param registry ECS 注册表
 */
void Sys_DeathJudgment::OnDestroy(Registry& registry) {
    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            if (m_TriggerSubId != 0)
                bus->unsubscribe<Evt_Phys_TriggerEnter>(m_TriggerSubId);
            if (m_CollisionSubId != 0)
                bus->unsubscribe<Evt_Phys_Collision>(m_CollisionSubId);
        }
    }
    m_TriggerSubId   = 0;
    m_CollisionSubId = 0;

    LOG_INFO("[Sys_DeathJudgment] OnDestroy");
}

} // namespace ECS
