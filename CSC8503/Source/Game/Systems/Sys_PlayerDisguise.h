#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_Physics;

/**
 * @brief 玩家伪装系统（优先级 59）
 *
 * 职责：
 *   - E 键切换纸箱伪装
 *   - 管理 C_T_Hidden 标签
 *   - 伪装时设置 forceStandPending=true（由 Sys_PlayerStance 下一帧执行站立）
 *
 * 执行顺序：在 Sys_PlayerStance(60) 之前，确保 forceStandPending 先设置
 *
 * 读：C_D_Input, C_D_RigidBody, C_D_PlayerState, Sys_Physics*
 * 写：C_D_PlayerState, C_T_Hidden
 */
class Sys_PlayerDisguise : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

};

} // namespace ECS
