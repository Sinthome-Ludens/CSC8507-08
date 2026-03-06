#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/// @brief CQC 近身制服系统（优先级 63）
///
/// 职责：背后接近检测 → 状态机推进 → 敌人休眠 → 拟态伪装
/// 读：C_D_Input, C_D_Transform, C_D_PlayerState, C_D_AIState, Res_CQCConfig
/// 写：C_D_CQCState, C_D_EnemyDormant, C_D_MeshRenderer
class Sys_PlayerCQC : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
