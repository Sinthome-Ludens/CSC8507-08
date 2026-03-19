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

};

} // namespace ECS
