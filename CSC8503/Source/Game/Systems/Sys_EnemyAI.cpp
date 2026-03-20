/**
 * @file Sys_EnemyAI.cpp
 * @brief 敌人 AI 逻辑系统实现：感知警戒度增减、四状态切换、噪音感知。
 *
 * @details
 * 遍历所有具有 C_T_Enemy + C_D_AIState + C_D_AIPerception 的实体，
 * 根据 is_spotted 增减 detection_value，并按阈值切换 Safe/Search/Alert/Hunt 状态。
 *
 * OnAwake 订阅 Evt_Player_Noise 实现听觉感知：
 * 玩家产生噪音时，按 XZ 距离衰减 boost 附近敌人的 detection_value。
 * 参数由 Res_AIConfig（noise_hearing_range, noise_boost_factor）数据驱动。
 *
 * 改进项（相对初始版本）：
 *   - 硬编码参数提取到 Res_AIConfig 资源
 *   - 状态切换添加 hysteresis_band 防阈值边界翻转
 *   - detection_value 增速按 spotted_distance / maxDistance 衰减
 *   - EventBus 订阅 Evt_Player_Noise 实现噪音感知
 */
#include "Sys_EnemyAI.h"
#include "Game/Utils/PauseGuard.h"
#include <algorithm>
#include <cmath>
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/Res_EnemyEnums.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_D_DDoSFrozen.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_AIConfig.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Events/Evt_Player_Noise.h"
#include "Game/Events/Evt_EnemyAlertChange.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_EnemyAI::OnAwake(Registry& registry) {
    if (!registry.has_ctx<EventBus*>()) return;
    auto* bus = registry.ctx<EventBus*>();
    if (!bus) return;

    m_NoiseSubId = bus->subscribe<Evt_Player_Noise>(
        [&registry](const Evt_Player_Noise& e) {
            Res_AIConfig cfg;
            if (registry.has_ctx<Res_AIConfig>()) {
                cfg = registry.ctx<Res_AIConfig>();
            }

            const float hearRangeSq = cfg.noise_hearing_range * cfg.noise_hearing_range;

            registry.view<C_T_Enemy, C_D_AIPerception, C_D_Transform>().each(
                [&](EntityID enemyId, C_T_Enemy&, C_D_AIPerception& perception, C_D_Transform& etf) {
                    if (registry.Has<C_D_EnemyDormant>(enemyId)) {
                        auto& dormant = registry.Get<C_D_EnemyDormant>(enemyId);
                        if (dormant.isDormant) return;
                    }

                    float dx = e.position.x - etf.position.x;
                    float dz = e.position.z - etf.position.z;
                    float distSq = dx * dx + dz * dz;

                    if (distSq >= hearRangeSq) return;

                    float dist = std::sqrt(distSq);
                    float falloff = 1.0f - (dist / cfg.noise_hearing_range);
                    float boost = e.volume * cfg.noise_boost_factor * falloff;
                    perception.detection_value = std::min(
                        perception.detection_value + boost, cfg.maxDetectionValue);
                });
        });

    /**
     * 敌人间通信：收到 Evt_EnemyAlertChange 后，
     * 在 ally_alert_range 内的其他敌人 detection_value 直接 boost ally_alert_boost，
     * 模拟呼叫增援 / 群体联动行为。
     */
    m_AlertSubId = bus->subscribe<Evt_EnemyAlertChange>(
        [&registry](const Evt_EnemyAlertChange& e) {
            if (e.newState != EnemyState::Hunt) return;

            Res_AIConfig cfg;
            if (registry.has_ctx<Res_AIConfig>()) {
                cfg = registry.ctx<Res_AIConfig>();
            }

            const float rangeSq = cfg.ally_alert_range * cfg.ally_alert_range;

            registry.view<C_T_Enemy, C_D_AIPerception, C_D_Transform>().each(
                [&](EntityID enemyId, C_T_Enemy&, C_D_AIPerception& perception, C_D_Transform& etf) {
                    if (enemyId == e.source) return;

                    if (registry.Has<C_D_EnemyDormant>(enemyId)) {
                        auto& dormant = registry.Get<C_D_EnemyDormant>(enemyId);
                        if (dormant.isDormant) return;
                    }

                    float dx = e.position.x - etf.position.x;
                    float dz = e.position.z - etf.position.z;
                    if (dx * dx + dz * dz < rangeSq) {
                        perception.detection_value = std::min(
                            perception.detection_value + cfg.ally_alert_boost, cfg.maxDetectionValue);
                    }
                });
        });
}

void Sys_EnemyAI::OnDestroy(Registry& registry) {
    if (!registry.has_ctx<EventBus*>()) return;
    auto* bus = registry.ctx<EventBus*>();
    if (!bus) return;
    bus->unsubscribe<Evt_Player_Noise>(m_NoiseSubId);
    bus->unsubscribe<Evt_EnemyAlertChange>(m_AlertSubId);
}

void Sys_EnemyAI::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
    Res_AIConfig aiCfg;
    if (registry.has_ctx<Res_AIConfig>()) {
        aiCfg = registry.ctx<Res_AIConfig>();
    }

    Res_VisionConfig defaultVisCfg;
    const auto& visCfg = registry.has_ctx<Res_VisionConfig>() ? registry.ctx<Res_VisionConfig>() : defaultVisCfg;
    const float visionMaxDist = visCfg.maxDistance;

    const float contactDistSq = aiCfg.contact_distance * aiCfg.contact_distance;
    const float hyst           = aiCfg.hysteresis_band;

    NCL::Maths::Vector3 playerPos{};
    bool hasPlayer = false;
    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID, C_T_Player&, C_D_Transform& ptf) {
            playerPos = ptf.position;
            hasPlayer = true;
        });

    if (!m_DidLogStartupState) {
        int enemyCount = 0;
        registry.view<C_T_Enemy>().each([&](EntityID, C_T_Enemy&) { ++enemyCount; });

        int matchPhase = -1;
        bool isMultiplayer = false;
        if (registry.has_ctx<Res_GameState>()) {
            const auto& gs = registry.ctx<Res_GameState>();
            matchPhase = static_cast<int>(gs.matchPhase);
            isMultiplayer = gs.isMultiplayer;
        }

        LOG_MPDBG("[Sys_EnemyAI] Startup snapshot: hasPlayer=" << hasPlayer
                  << " enemyCount=" << enemyCount
                  << " isMultiplayer=" << isMultiplayer
                  << " matchPhase=" << matchPhase);
        m_DidLogStartupState = true;
    }

    auto view = registry.view<C_T_Enemy, C_D_AIState, C_D_AIPerception>();

    view.each([&](EntityID entity, C_T_Enemy&, C_D_AIState& state, C_D_AIPerception& detect) {
        if (registry.Has<C_D_EnemyDormant>(entity)) {
            auto& dormant = registry.Get<C_D_EnemyDormant>(entity);
            if (dormant.isDormant) return;
        }

        // DDoS 冻结：只做警戒衰减（让解冻后不会立刻 Hunt），跳过其余 AI 逻辑
        if (registry.Has<C_D_DDoSFrozen>(entity)) {
            detect.is_spotted = false;
            detect.detection_value -= detect.detection_value_decrease * dt;
            detect.detection_value = std::max(0.0f, detect.detection_value);
            if (detect.detection_value < detect.search_threshold && state.current_state != EnemyState::Safe) {
                state.current_state = EnemyState::Safe;
                LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " DDoS freeze -> Safe");
            }
            return;
        }

        /**
         * 接触感知：XZ 距离小于 contact_distance 时将警戒拉到 Alert 级别。
         * 不直接进入 Hunt，给玩家一个反应窗口。
         */
        if (hasPlayer && registry.Has<C_D_Transform>(entity)) {
            const auto& etf = registry.Get<C_D_Transform>(entity);
            float dx = playerPos.x - etf.position.x;
            float dz = playerPos.z - etf.position.z;
            float contactDistSqActual = dx * dx + dz * dz;
            if (contactDistSqActual < contactDistSq) {
                if (detect.detection_value < detect.alert_threshold) {
                    detect.detection_value = detect.alert_threshold;
                }
                detect.is_spotted = true;
                detect.spotted_distance = std::sqrt(contactDistSqActual);
            }
        }

        /**
         * Hunt 锁定计时器无条件倒计时（spotted 时也扣），截断到 0。
         * 锁定期间警戒度强制 >= hunt_threshold，防止视线短暂中断导致状态掉落。
         */
        if (detect.hunt_lock_timer > 0.0f)
            detect.hunt_lock_timer = std::max(0.0f, detect.hunt_lock_timer - dt);

        if (detect.is_spotted) {
            /**
             * 距离调制：远处的瞥见累积更慢。
             * factor = 1.0 - 0.7 * (spotted_distance / maxDistance)，
             * 即 0m 处增速为 100%，maxDistance 处增速为 30%。
             * clamp 在 [0.3, 1.0] 确保不会降为零。
             */
            float distFactor = aiCfg.maxDistFactor;
            if (visionMaxDist > 0.001f) {
                distFactor = aiCfg.maxDistFactor - aiCfg.visionDistModulation * (detect.spotted_distance / visionMaxDist);
                distFactor = std::clamp(distFactor, aiCfg.minDistFactor, aiCfg.maxDistFactor);
            }
            detect.detection_value += detect.detection_value_increase * distFactor * dt;
        } else {
            if (state.current_state == EnemyState::Hunt && detect.hunt_lock_timer > 0.0f) {
                if (detect.detection_value < detect.hunt_threshold) {
                    detect.detection_value = detect.hunt_threshold;
                }
            } else {
                detect.detection_value -= detect.detection_value_decrease * dt;
            }
        }

        detect.detection_value = std::clamp(detect.detection_value, 0.0f, aiCfg.maxDetectionValue);

        /**
         * 状态切换（带 hysteresis）：
         * 上升沿使用原始阈值，下降沿使用 (阈值 - hysteresis_band)。
         * 防止 detection_value 在阈值边界振荡时状态快速翻转。
         */
        EnemyState nextState = state.current_state;
        const float v = detect.detection_value;

        if (v >= detect.hunt_threshold) {
            if (state.current_state != EnemyState::Hunt) {
                detect.hunt_lock_timer = aiCfg.hunt_lock_duration;
                if (registry.has_ctx<Res_GameState>()) {
                    auto& gs = registry.ctx<Res_GameState>();
                    gs.alertLevel = std::min(gs.alertLevel + aiCfg.global_alert_increment, gs.alertMax);
                }
                LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " -> HUNT (lock " << aiCfg.hunt_lock_duration << "s)");

                if (registry.has_ctx<EventBus*>()) {
                    auto* evtBus = registry.ctx<EventBus*>();
                    if (evtBus && registry.Has<C_D_Transform>(entity)) {
                        const auto& etf = registry.Get<C_D_Transform>(entity);
                        evtBus->publish_deferred(Evt_EnemyAlertChange{entity, EnemyState::Hunt, etf.position});
                    }
                }
            }
            nextState = EnemyState::Hunt;
        } else if (state.current_state == EnemyState::Hunt && v >= detect.hunt_threshold - hyst) {
            nextState = EnemyState::Hunt;
        } else if (v >= detect.alert_threshold) {
            nextState = EnemyState::Alert;
        } else if (state.current_state == EnemyState::Alert && v >= detect.alert_threshold - hyst) {
            nextState = EnemyState::Alert;
        } else if (v >= detect.search_threshold) {
            nextState = EnemyState::Search;
        } else if (state.current_state == EnemyState::Search && v >= detect.search_threshold - hyst) {
            nextState = EnemyState::Search;
        } else {
            nextState = EnemyState::Safe;
        }

        if (nextState != state.current_state) {
            state.current_state = nextState;
            LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " state -> " << (int)nextState);
        }
    });
}

} // namespace ECS
