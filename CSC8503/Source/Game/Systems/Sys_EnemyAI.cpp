#include "Sys_EnemyAI.h"
#include <algorithm>
#include "Game/Components/C_D_AIPreception.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Utils/Log.h"

namespace ECS {
    void Sys_EnemyAI::OnUpdate(Registry& registry, float dt) {
        auto view = registry.view<C_T_Enemy, C_D_AIState, C_D_AIPreception>();

        view.each([&](EntityID entity, C_T_Enemy& tag, C_D_AIState& state, C_D_AIPreception& detect) {

            // --- 1. 警戒度增减逻辑 ---
            if (detect.isSpotted) {
                // 玩家在视野内：无论什么状态，警戒度都持续上升
                detect.detectionValue += detect.detectionValueIncrease * dt;
            }
            else {
                // 玩家不在视野内：
                // 如果处于 Hunt 状态且 5 秒锁定计时器未耗尽，则警戒度不下降
                if (state.currentState == EnemyState::Hunt && detect.huntLockTimer > 0.0f) {
                    detect.huntLockTimer -= dt;
                } else {
                    // 其他情况（计时器耗尽或非 Hunt 状态），正常下降
                    detect.detectionValue -= detect.detectionValueDecrease * dt;
                }
            }

            // 强制限制警戒度最高 100，最低 0
            detect.detectionValue = std::clamp(detect.detectionValue, 0.0f, 100.0f);

            // --- 2. 状态机切换逻辑 ---
            EnemyState nextState = state.currentState;
            float v = detect.detectionValue;

            if (v >= 50.0f) {
                // 达到或超过 50 进入 Hunt
                if (state.currentState != EnemyState::Hunt) {
                    detect.huntLockTimer = 5.0f; // 刚进入 Hunt 的瞬间，重置 5 秒锁定时间
                    LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " ENTERED HUNT! Locking detection for 5s.");
                }
                nextState = EnemyState::Hunt;
            }
            else if (v >= 30.0f) {
                nextState = EnemyState::Alert;
            }
            else if (v >= 15.0f) {
                nextState = EnemyState::Caution;
            }
            else {
                nextState = EnemyState::Safe;
            }

            // 更新状态
            if (nextState != state.currentState) {
                state.currentState = nextState;
                LOG_INFO("SYS_ENEMY_AI: Entity " << (int)entity << " state changed to " << (int)nextState);
            }

            // --- 3. 行为指令输出 (状态机表现) ---
            switch (state.currentState) {
                case EnemyState::Safe:
                    LOG_INFO("SYS_ENEMY_AI: [Entity " << (int)entity << "] [MOVE_CMD]: patrol | speed: 1.0");
                    break;
                case EnemyState::Caution:
                    LOG_INFO("SYS_ENEMY_AI: [Entity " << (int)entity << "] [MOVE_CMD]: face_player | hold");
                    break;
                case EnemyState::Alert:
                    LOG_INFO("SYS_ENEMY_AI: [Entity " << (int)entity << "] [MOVE_CMD]: move_to_last_pos | search_rotate");
                    break;
                case EnemyState::Hunt:
                    LOG_INFO("SYS_ENEMY_AI: [Entity " << (int)entity << "] [MOVE_CMD]: chasing_player | speed: 1.5");
                    break;
            }
        });
    }
}