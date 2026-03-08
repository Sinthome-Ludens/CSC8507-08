/**
 * @file Sys_DeathJudgment.cpp
 * @brief 死亡判定系统实现。
 *
 * 算法概要：
 * - OnAwake: 订阅 Evt_Phys_TriggerEnter，回调中检测 C_T_DeathZone 并将受害者 hp 设为 0
 * - OnUpdate:
 *   1. 更新无敌计时器（invTimer 递减）
 *   2. Hunt 敌人抓捕检测（XZ 平方距离 < captureDistance²）
 *   3. 死亡检查：玩家 hp<=0 触发场景重启，敌人 hp<=0 触发实体销毁
 * - OnDestroy: 取消 EventBus 订阅
 */
#include "Sys_DeathJudgment.h"

#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_T_DeathZone.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Events/Evt_Phys_Trigger.h"
#include "Game/Events/Evt_Death.h"
#include "Game/Scenes/IScene.h"
#include "Game/Utils/Log.h"

namespace ECS {

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
        }
    }

    LOG_INFO("[Sys_DeathJudgment] OnAwake");
}

void Sys_DeathJudgment::OnUpdate(Registry& registry, float dt) {
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

    const float captureDistSq = config.captureDistance * config.captureDistance;

    if (playerCount > 0) {
        registry.view<C_T_Enemy, C_D_AIState, C_D_Transform>().each(
            [&](EntityID enemyId, C_T_Enemy&, C_D_AIState& state, C_D_Transform& enemyTf) {
                // 只有 Hunt 状态的敌人才抓捕
                if (state.current_state != EnemyState::Hunt) return;

                // 休眠敌人跳过
                if (registry.Has<C_D_EnemyDormant>(enemyId)) {
                    const auto& dormant = registry.Get<C_D_EnemyDormant>(enemyId);
                    if (dormant.isDormant) return;
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
    bool playerDied = false;

    registry.view<C_D_Health>().each(
        [&](EntityID entity, C_D_Health& health) {
            if (health.hp > 0.0f) return;

            if (registry.Has<C_T_Player>(entity)) {
                // 玩家死亡 → 场景重启
                if (!playerDied) {
                    playerDied = true;
                    LOG_INFO("[DeathJudgment] Player " << (int)entity << " died, restarting scene");

                    if (registry.has_ctx<EventBus*>()) {
                        auto* bus = registry.ctx<EventBus*>();
                        if (bus) {
                            Evt_Death evt;
                            evt.entity    = entity;
                            evt.deathType = health.deathCause;
                            bus->publish_deferred(evt);
                        }
                    }

                    if (registry.has_ctx<IScene*>()) {
                        auto* scene = registry.ctx<IScene*>();
                        if (scene) scene->Restart();
                    }
                }
            } else if (registry.Has<C_T_Enemy>(entity)) {
                // 敌人死亡 → 销毁实体
                LOG_INFO("[DeathJudgment] Enemy " << (int)entity << " died, destroying");

                if (registry.has_ctx<EventBus*>()) {
                    auto* bus = registry.ctx<EventBus*>();
                    if (bus) {
                        Evt_Death evt;
                        evt.entity    = entity;
                        evt.deathType = DeathType::EnemyHpZero;
                        bus->publish_deferred(evt);
                    }
                }

                registry.Destroy(entity);
            }
        }
    );
}

void Sys_DeathJudgment::OnDestroy(Registry& registry) {
    if (m_TriggerSubId != 0 && registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            bus->unsubscribe<Evt_Phys_TriggerEnter>(m_TriggerSubId);
        }
    }
    m_TriggerSubId = 0;

    LOG_INFO("[Sys_DeathJudgment] OnDestroy");
}

} // namespace ECS
