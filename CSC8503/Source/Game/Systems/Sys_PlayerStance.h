#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_Physics;

/**
 * @brief 玩家姿态系统（优先级 60）
 *
 * 职责：
 *   - C 键蹲下 / V 键站起（Standing ↔ Crouching）
 *   - 碰撞体形状替换（Capsule 半高变化）
 *   - 脚底位置保持（Y 轴偏移补偿）
 *   - 发布 Evt_Player_StanceChanged 事件
 *   - 处理 forceStandPending 标志（由伪装/奔跑系统设置）
 *
 * 读：C_D_Input, C_D_Transform, C_D_RigidBody, C_D_PlayerState, Sys_Physics*
 * 写：C_D_PlayerState
 */
class Sys_PlayerStance : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

private:
    // ── 碰撞体参数 ──
    static constexpr float CAPSULE_RADIUS     = 0.5f;
    static constexpr float STAND_HALF_HEIGHT  = 1.0f;
    static constexpr float CROUCH_HALF_HEIGHT = 0.5f;
};

} // namespace ECS
