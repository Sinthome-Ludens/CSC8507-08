/**
 * @file Sys_PlayerStance.h
 * @brief 玩家姿态系统声明：站立 ↔ 蹲伏切换及 Box 碰撞体重建。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_Physics;

/**
 * @brief 玩家姿态系统（优先级 60）
 *
 * 职责：
 *   - C 键蹲下 / V 键站起（Standing ↔ Crouching）
 *   - 碰撞体形状替换（当前玩家使用 Box 的站立/下蹲变化）
 *   - 脚底位置保持（Y 轴偏移补偿）
 *   - 发布 Evt_Player_StanceChanged 事件
 *   - 处理 forceStandPending 标志（由伪装/奔跑系统设置）
 *
 * 读：C_D_Input, C_D_Transform, C_D_RigidBody, C_D_PlayerState, Sys_Physics*
 * 写：C_D_PlayerState
 */
class Sys_PlayerStance : public ISystem {
public:
    /**
     * @brief 每帧处理玩家姿态输入，切换 Box 碰撞体形状并补偿脚底 Y 位置。
     * @param registry 当前场景注册表
     * @param dt       本帧时间步长（当前实现未直接使用）
     */
    void OnUpdate(Registry& registry, float dt) override;

};

} // namespace ECS
