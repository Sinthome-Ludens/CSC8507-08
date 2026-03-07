#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/// @brief 敌人视野判定系统（优先级 110）
///
/// 职责：每帧为每个活跃敌人计算 is_spotted，XZ 平面扇形视锥 + 射线遮挡
/// 读：C_D_Transform, C_D_PlayerState, C_D_CQCState, C_T_Player, C_T_Enemy,
///     C_D_EnemyDormant, Res_VisionConfig, Sys_Physics*
/// 写：C_D_AIPerception.is_spotted
class Sys_EnemyVision : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
