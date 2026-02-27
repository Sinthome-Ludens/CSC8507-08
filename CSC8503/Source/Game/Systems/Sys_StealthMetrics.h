#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_Physics;

/**
 * @brief 潜行指标系统（优先级 62）
 *
 * 职责：
 *   - 奔跑判定 + moveSpeedMul 计算
 *   - 奔跑强制站起：设 forceStandPending=true
 *   - 噪音/可见度计算
 *   - 噪音事件发布（带 0.3s 节流）
 *
 * 读：C_D_Input, C_D_Transform, C_D_RigidBody, C_D_PlayerState, Sys_Physics*
 * 写：C_D_PlayerState
 */
class Sys_StealthMetrics : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

private:
    // ── 姿态速度乘数 ──
    static constexpr float STANCE_MUL_STANDING  = 1.0f;
    static constexpr float STANCE_MUL_CROUCHING = 0.5f;

    // ── 伪装常量 ──
    static constexpr float DISGUISE_MUL   = 0.3f;
    static constexpr float NOISE_THROTTLE = 0.3f;

    // ── 噪音节流计时器 ──
    float m_NoiseCooldown = 0.0f;
};

} // namespace ECS
