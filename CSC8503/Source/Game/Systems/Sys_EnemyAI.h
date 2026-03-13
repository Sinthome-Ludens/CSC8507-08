/**
 * @file Sys_EnemyAI.h
 * @brief 敌人 AI 逻辑系统声明（ECS 系统，优先级 120）。
 *
 * @see Sys_EnemyAI.cpp
 */
#pragma once
#include "Core/ECS/BaseSystem.h"

namespace ECS {
    /**
     * @brief 敌人 AI 逻辑系统
     * 职责：管理感知警戒度（detection_value）增减及四状态（Safe/Search/Alert/Hunt）切换
     * 执行优先级：120（晚于 Sys_Physics，早于 Sys_Render）
     */
    class Sys_EnemyAI : public ISystem {
    public:
        Sys_EnemyAI() = default;

        /**
         * @brief 每帧更新所有敌人的 AI 状态：警戒度增减与四状态切换。
         * @param registry ECS 注册表
         * @param dt       帧时间（秒）
         */
        void OnUpdate(Registry& registry, float dt) override;

    };
}