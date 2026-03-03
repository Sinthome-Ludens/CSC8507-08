#pragma once
#include "Core/ECS/BaseSystem.h"

namespace ECS {
    /**
     * @brief 敌人 AI 逻辑系统
     * 职责：管理感知警戒度（detection_value）增减及四状态（Safe/Caution/Alert/Hunt）切换
     * 执行优先级：120（晚于 Sys_Physics，早于 Sys_Render）
     */
    class Sys_EnemyAI : public ISystem {
    public:
        Sys_EnemyAI() = default;

        void OnUpdate(Registry& registry, float dt) override;

    };
}