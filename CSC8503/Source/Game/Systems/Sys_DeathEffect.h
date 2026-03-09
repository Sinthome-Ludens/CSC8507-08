/**
 * @file Sys_DeathEffect.h
 * @brief 死亡视觉特效系统声明（ECS 系统，优先级 126）。
 *
 * 处理敌人死亡动画：数字冲击 → 霓虹故障 → 数据溶解 → 最终崩塌 → 销毁实体。
 * 在 Sys_DeathJudgment(125) 之后执行，读取 C_D_Dying 计时器推进动画。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_DeathEffect : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
