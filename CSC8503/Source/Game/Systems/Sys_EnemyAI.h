/**
 * @file Sys_EnemyAI.h
 * @brief 敌人 AI 逻辑系统声明（ECS 系统，优先级 120）。
 *
 * 职责：管理感知警戒度（detection_value）增减及四状态（Safe/Search/Alert/Hunt）切换。
 * 订阅 Evt_Player_Noise 事件实现听觉感知（按距离衰减 boost detection_value）。
 */
#pragma once
#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"

namespace ECS {

class Sys_EnemyAI : public ISystem {
public:
    Sys_EnemyAI() = default;

    void OnAwake  (Registry& registry) override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override;

private:
    SubscriptionID m_NoiseSubId = 0;
    SubscriptionID m_AlertSubId = 0;
    bool m_DidLogStartupState = false;
};

} // namespace ECS
