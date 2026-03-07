#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_Physics;

/**
 * @brief 移动系统（优先级 65）
 *
 * 职责：纯物理移动——读取方向和速度参数，施加力/制动。
 * 不做任何游戏规则判定，仅执行物理操作。
 *
 * 读：C_D_Input, C_D_RigidBody, C_D_PlayerState, Sys_Physics*
 * 写：无（通过 Sys_Physics API 操作刚体）
 */
class Sys_Movement : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

private:
    // ── 速度常量 ──
    static constexpr float BASE_SPEED    = 5.0f;    ///< 基准速度（站立行走）
    static constexpr float BASE_FORCE    = 80.0f;   ///< 基准驱动力
    static constexpr float RUN_SPEED_MUL = 1.5f;    ///< 奔跑倍率 → 7.5
};

} // namespace ECS
