/**
 * @file Sys_Countdown.h
 * @brief Countdown system — triggers game-over when alert level reaches maximum
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 倒计时逻辑系统
 *
 * 触发条件：alertLevel >= alertMax 且 countdownActive == false
 * 每帧递减 countdownTimer，归零时设置 gameOverReason=1 并切换到 GameOver 画面。
 *
 * 注册优先级：350（在 AI/游戏逻辑之后、UI 之前）
 * ECS 合规：System 无成员变量，仅读写 Res_GameState 和 Res_UIState。
 */
class Sys_Countdown : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
