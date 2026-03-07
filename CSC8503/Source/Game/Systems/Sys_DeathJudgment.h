#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"

namespace ECS {

/**
 * @brief 死亡判定系统（优先级 125）
 *
 * 三种死亡触发源：
 *   1. Hunt 敌人抓捕（XZ 距离 < captureDistance）
 *   2. HP 归零（C_D_Health.hp <= 0）
 *   3. 触发器区域即死（Evt_Phys_TriggerEnter + C_T_DeathZone）
 *
 * 玩家死亡 → 场景重启（IScene::Restart）
 * 敌人死亡 → registry.Destroy（延迟销毁）
 */
class Sys_DeathJudgment : public ISystem {
public:
    void OnAwake  (Registry& registry) override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override;

private:
    SubscriptionID m_TriggerSubId = 0;
};

} // namespace ECS
