#include "Sys_EnemyAI.h"
#include <algorithm>
#include "Game/Components/C_D_AIPreception.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Utils/Log.h"

namespace ECS {
    void Sys_EnemyAI::OnUpdate(Registry& registry, float dt) {
        // 仅依赖 AI 核心组件，不要求寻路组件（寻路由独立系统处理）
        auto view = registry.view<C_T_Enemy, C_D_AIState, C_D_AIPreception>();

        view.each([&](EntityID entity, C_T_Enemy&, C_D_AIState& state, C_D_AIPreception& detect) {

            // ── 1. 警戒度增减 ────────────────────────────────────────────
            if (detect.isSpotted) {
                // 玩家在视野内：警戒度持续上升
                detect.detectionValue += detect.detectionValueIncrease * dt;
            } else {
                if (state.currentState == EnemyState::Hunt && detect.huntLockTimer > 0.0f) {
                    // Hunt 锁定期：只倒计时，警戒度不下降，并强制 >= 50
                    detect.huntLockTimer -= dt;
                    if (detect.detectionValue < 50.0f) {
                        detect.detectionValue = 50.0f;
                    }
                } else {
                    // 普通下降
                    detect.detectionValue -= detect.detectionValueDecrease * dt;
                }
            }

            // 限制 [0, 100]
            detect.detectionValue = std::clamp(detect.detectionValue, 0.0f, 100.0f);

            // ── 2. 状态切换 ──────────────────────────────────────────────
            EnemyState nextState = state.currentState;
            const float v = detect.detectionValue;

            if (v >= 50.0f) {
                if (state.currentState != EnemyState::Hunt) {
                    detect.huntLockTimer = 5.0f; // 进入 Hunt 时启动 5 秒锁定
                    LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " -> HUNT (lock 5s)");
                }
                nextState = EnemyState::Hunt;
            } else if (v >= 30.0f) {
                nextState = EnemyState::Alert;
            } else if (v >= 15.0f) {
                nextState = EnemyState::Caution;
            } else {
                nextState = EnemyState::Safe;
            }

            if (nextState != state.currentState) {
                state.currentState = nextState;
                LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " state -> " << (int)nextState);
            }
        });
    }
} // namespace ECS
