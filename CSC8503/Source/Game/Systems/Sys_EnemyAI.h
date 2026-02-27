#pragma once
#include "Core/ECS/BaseSystem.h"

namespace ECS {
    /**
     * @brief 敌人 AI 逻辑系统
     * 职责：处理状态切换、侦测值计算及非暴力交互逻辑
     * 执行优先级：120 (晚于 Sys_Physics)
     */
    class Sys_EnemyAI : public ISystem {
    public:
        Sys_EnemyAI() = default;

        void OnUpdate(Registry& registry, float dt) override;

    private:
        void UpdateAIState(Registry& registry, EntityID entity, float dt);
        void UpdateHackingLogic(Registry& registry, EntityID entity, float dt);
    };
}