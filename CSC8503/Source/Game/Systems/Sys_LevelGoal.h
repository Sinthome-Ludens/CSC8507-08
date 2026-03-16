/**
 * @file Sys_LevelGoal.h
 * @brief 关卡目标系统：检测玩家到达终点区域，触发过关/胜利。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 关卡目标系统 — 每帧检测玩家与 C_T_FinishZone 实体的 XZ 距离
 *
 * 当玩家进入终点 4m 半径内，设置 Res_GameState::gameOverReason = 3（任务成功）。
 * 执行优先级：127（晚于 Sys_DeathEffect 126）
 */
class Sys_LevelGoal : public ISystem {
public:
    /** @brief 初始化标志位。 */
    void OnAwake(Registry& registry) override;

    /** @brief 每帧检测玩家与终点实体的 XZ 距离，触发胜利。 */
    void OnUpdate(Registry& registry, float dt) override;

    /** @brief 清理资源。 */
    void OnDestroy(Registry& registry) override;

private:
    bool m_FinishTriggered = false;  ///< 防止重复触发
};

} // namespace ECS
