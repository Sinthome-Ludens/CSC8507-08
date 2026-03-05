#include "Sys_EnemyAI.h"
#include <algorithm>
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Utils/Log.h"

namespace ECS {
    void Sys_EnemyAI::OnUpdate(Registry& registry, float dt) {
        // 仅依赖 AI 核心组件，不要求寻路组件（寻路由独立系统处理）
        auto view = registry.view<C_T_Enemy, C_D_AIState, C_D_AIPerception>();

        view.each([&](EntityID entity, C_T_Enemy&, C_D_AIState& state, C_D_AIPerception& detect) {

            // ── 1. 警戒度增减 ────────────────────────────────────────────
            // Hunt 锁定计时器无条件倒计时（spotted 时也扣），截断到 0
            if (detect.hunt_lock_timer > 0.0f)
                detect.hunt_lock_timer = std::max(0.0f, detect.hunt_lock_timer - dt);

            if (detect.is_spotted) {
                // 玩家在视野内：警戒度持续上升
                detect.detection_value += detect.detection_value_increase * dt;
            } else {
                if (state.current_state == EnemyState::Hunt && detect.hunt_lock_timer > 0.0f) {
                    // Hunt 锁定期：警戒度不下降，并强制 >= hunt_threshold
                    if (detect.detection_value < detect.hunt_threshold) {
                        detect.detection_value = detect.hunt_threshold;
                    }
                } else {
                    // 普通下降
                    detect.detection_value -= detect.detection_value_decrease * dt;
                }
            }

            // 限制 [0, 100]
            detect.detection_value = std::clamp(detect.detection_value, 0.0f, 100.0f);

            // ── 2. 状态切换 ──────────────────────────────────────────────
            EnemyState nextState = state.current_state;
            const float v = detect.detection_value;

            if (v >= detect.hunt_threshold) {
                if (state.current_state != EnemyState::Hunt) {
                    detect.hunt_lock_timer = 5.0f; // 进入 Hunt 时启动 5 秒锁定
                    LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " -> HUNT (lock 5s)");
                }
                nextState = EnemyState::Hunt;
            } else if (v >= detect.alert_threshold) {
                nextState = EnemyState::Alert;
            } else if (v >= detect.caution_threshold) {
                nextState = EnemyState::Caution;
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
