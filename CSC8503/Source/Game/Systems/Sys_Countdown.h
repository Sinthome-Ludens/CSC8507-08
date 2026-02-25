#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 倒计时逻辑系统
 *
 * 职责：
 *   - 当 alertLevel 达到阈值（150.0）且 !countdownActive 时启动倒计时
 *   - 每帧递减 countdownTimer
 *   - countdownTimer <= 0 → 设置 gameOverReason=1，activeScreen=GameOver
 *
 * 执行优先级：350（物理之后、Chat之前）
 */
class Sys_Countdown : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
