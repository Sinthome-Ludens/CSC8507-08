/**
 * @file Sys_DeathJudgment.h
 * @brief 死亡判定系统声明（ECS 系统，优先级 125）。
 *
 * 检测三种死亡触发源（敌人抓捕/HP归零/触发区即死），
 * 玩家死亡触发 GameOver 界面，敌人死亡挂载死亡组件由 Sys_DeathEffect 处理。
 */
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
 * 玩家死亡 → GameOver 界面（UIScreen::GameOver）
 * 敌人死亡 → 挂载 C_D_Dying + C_D_DeathVisual，由 Sys_DeathEffect 执行动画后延迟销毁
 */
class Sys_DeathJudgment : public ISystem {
public:
    /**
     * @brief 系统初始化，订阅触发器进入事件。
     * @param registry ECS 注册表，用于获取 EventBus 和注册订阅。
     */
    void OnAwake  (Registry& registry) override;

    /**
     * @brief 每帧更新：无敌计时器递减、Hunt 抓捕检测、死亡检查与处理。
     * @param registry ECS 注册表，用于访问 C_D_Health、C_D_AIState 等组件。
     * @param dt 本帧经过的时间（秒）。
     */
    void OnUpdate (Registry& registry, float dt) override;

    /**
     * @brief 系统销毁，取消触发器事件订阅。
     * @param registry ECS 注册表，用于获取 EventBus 取消订阅。
     */
    void OnDestroy(Registry& registry) override;

private:
    SubscriptionID m_TriggerSubId   = 0;
    SubscriptionID m_CollisionSubId = 0;
    bool m_DidLogStartupState = false;
};

} // namespace ECS
