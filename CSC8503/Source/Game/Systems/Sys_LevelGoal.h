/**
 * @file Sys_LevelGoal.h
 * @brief 关卡目标系统：检测玩家到达终点区域，触发过关/胜利。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_LevelGoal : public ISystem {
public:
    void OnAwake(Registry& registry) override;
    void OnUpdate(Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override;

private:
    bool m_FinishTriggered = false;
};

} // namespace ECS
