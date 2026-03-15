#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/// @brief CQC 近身制服系统（优先级 63）
///
/// 职责：范围内目标扫描 → 滚轮切换选中 → 高亮管理 → 状态机推进 → 击杀
/// 读：C_D_Input, C_D_Transform, C_D_PlayerState, C_D_AIState, Res_CQCConfig
/// 写：C_D_CQCState, C_D_CQCHighlight
class Sys_PlayerCQC : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
