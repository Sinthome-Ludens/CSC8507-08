#include "Sys_EnemyAI.h"

// 核心 ECS 组件
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPreception.h"

// 项目工具类
#include "Game/Utils/Log.h"
#include "Game/Utils/Assert.h"

namespace ECS {

/**
 * @brief 敌人 AI 系统实现
 * * 遵循项目规范：
 * 1. 逻辑与数据完全分离。
 * 2. 仅处理 C_T_Enemy 标记的实体。
 * 3. 移动逻辑已替换为全大写前缀的 SYS_ 日志输出。
 */
void Sys_EnemyAI::OnUpdate(Registry& registry, float dt) {
    // 获取所有具有敌人标签、状态数据和侦测数据的实体
    auto view = registry.view<C_T_Enemy, C_D_AIState, C_D_AIPreception>();

    /**
     * @note 修正后的 each 调用：
     * 参数列表必须严格匹配：(EntityID, view中第1个组件&, view中第2个组件&, ...)
     * 即使 C_T_Enemy 是空结构体标签，也必须作为参数传入。
     */
    view.each([&](EntityID entity, C_T_Enemy& tag, C_D_AIState& state, C_D_AIPreception& detect) {

        EnemyState nextState = state.currentState;

        // 状态判定逻辑
        if (detect.detectionValue > 70.0f) {
            nextState = EnemyState::Alert;
        }
        else if (detect.detectionValue > 15.0f) {
            nextState = EnemyState::Caution;
        }
        else {
            nextState = EnemyState::Safe;
        }

        // 只有状态改变时才打印日志，避免刷屏
        if (nextState != state.currentState) {
            LOG_INFO("SYS_ENEMY_AI: Entity %d Changed [%d -> %d]" << (int)entity << (int)state.currentState << (int)nextState);
            state.currentState = nextState;
        }

        // --- 2. 行为指令输出 (替代原移动逻辑) ---
        // 根据文档要求，此处不执行物理位移，仅输出意图字符串
        switch (state.currentState) {
            case EnemyState::Safe:
                // 对应原逻辑中的 Patrol
                LOG_INFO("SYS_ENEMY_AI: [Entity %d] [MOVE_CMD]: patrol | speed: 1.0" << (int)entity);
                break;

            case EnemyState::Caution:
                // 对应原逻辑中的 Stop / SweepRotate
                LOG_INFO("SYS_ENEMY_AI: [Entity %d] [MOVE_CMD]: guarding | hold" << (int)entity);
                break;

            case EnemyState::Alert:
                // 对应原逻辑中的 ChasePlayer
                LOG_INFO("SYS_ENEMY_AI: [Entity %d] [MOVE_CMD]: hunt | target: PLAYER_LAST_POS | speed: 1.5" << (int)entity);
                break;

            default:
                GAME_ASSERT(false, "SYS_ENEMY_AI: error");
                break;
        }

        // --- 3. 侦测值自然衰减 (模拟逻辑) ---
        if (!detect.isSpotted && detect.detectionValue > 0.0f) {
            detect.detectionValue -= detect.detectionValueDecrease * dt;
            detect.detectionValue = (detect.detectionValue < 0.0f) ? 0.0f : detect.detectionValue;
        }
    });
}

} // namespace ECS