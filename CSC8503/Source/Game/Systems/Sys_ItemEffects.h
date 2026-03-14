/**
 * @file Sys_ItemEffects.h
 * @brief 道具效果系统声明（优先级 260）：订阅 Evt_Item_Use，执行五种道具的具体效果逻辑。
 *
 * @details
 * 各道具效果职责：
 *
 * | 道具 ID      | 效果                                                              |
 * |-------------|-------------------------------------------------------------------|
 * | HoloBait    | 在目标位置创建诱饵实体（C_D_HoloBaitState），吸引附近安全状态敌人   |
 * | PhotonRadar | 激活/更新 Res_RadarState，每 3 秒刷新敌人位置                     |
 * | DDoS        | 对目标最近的敌人挂载 C_D_DDoSFrozen（5 秒冻结）                   |
 * | RoamAI      | 创建流窜 AI 实体（C_T_RoamAI + C_D_RoamAI），在地图随机巡逻       |
 * | TargetStrike| 将目标最近敌人的 hp 设为 0，触发 Sys_DeathJudgment 死亡判定       |
 *
 * Sys_ItemEffects 同时在 OnUpdate 中每帧推进：
 *  - HoloBait 诱饵计时器（到期销毁诱饵实体）
 *  - DDoS 冻结计时器（到期移除 C_D_DDoSFrozen）
 *  - RoamAI 随机游走（更新目标位置，检测敌人碰撞）
 *  - Radar 刷新计时器（到期重新扫描敌人位置）
 *
 * @see Evt_Item_Use.h
 * @see C_D_HoloBaitState.h
 * @see C_D_DDoSFrozen.h
 * @see C_D_RoamAI.h
 * @see Res_RadarState.h
 */
#pragma once

#include <cstdint>
#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"
#include "Vector.h"

namespace ECS {

/**
 * @brief 道具效果执行系统（优先级 260）
 *
 * 读：Evt_Item_Use（via EventBus），C_D_Transform，C_T_Enemy，C_D_Health，
 *     C_D_AIState，C_D_HoloBaitState，C_D_DDoSFrozen，C_D_RoamAI，C_T_RoamAI，
 *     Res_RadarState
 * 写：C_D_Health（TargetStrike/RoamAI 消灭敌人），C_D_DDoSFrozen（DDoS 冻结），
 *     C_D_HoloBaitState，C_D_RoamAI，Res_RadarState
 */
class Sys_ItemEffects : public ISystem {
public:
    /**
     * @brief 订阅 Evt_Item_Use 事件，初始化 Res_RadarState。
     * @param registry ECS 注册表。
     */
    void OnAwake(Registry& registry) override;

    /**
     * @brief 每帧推进各道具效果的持续状态（计时器、移动、雷达刷新）。
     * @param registry ECS 注册表。
     * @param dt       帧时间（秒）。
     */
    void OnUpdate(Registry& registry, float dt) override;

    /**
     * @brief 取消事件订阅。
     * @param registry ECS 注册表。
     */
    void OnDestroy(Registry& registry) override;

private:
    SubscriptionID m_UseSubId = 0; ///< Evt_Item_Use 订阅句柄

    // ── 各道具效果处理函数 ──

    /**
     * @brief 处理全息诱饵炸弹（HoloBait）使用效果：在目标位置创建诱饵实体。
     * @param registry ECS 注册表。
     * @param evt      道具使用事件。
     */
    void EffectHoloBait(Registry& registry, const struct Evt_Item_Use& evt);

    /**
     * @brief 处理光子雷达（PhotonRadar）使用效果：激活雷达并立即执行一次扫描。
     * @param registry ECS 注册表。
     * @param evt      道具使用事件。
     */
    void EffectPhotonRadar(Registry& registry, const struct Evt_Item_Use& evt);

    /**
     * @brief 处理 DDoS 使用效果：对使用者附近最近的敌人施加冻结状态。
     * @param registry ECS 注册表。
     * @param evt      道具使用事件。
     */
    void EffectDDoS(Registry& registry, const struct Evt_Item_Use& evt);

    /**
     * @brief 处理流窜 AI（RoamAI）使用效果：在玩家位置创建流窜 AI 实体。
     * @param registry ECS 注册表。
     * @param evt      道具使用事件。
     */
    void EffectRoamAI(Registry& registry, const struct Evt_Item_Use& evt);

    /**
     * @brief 处理靶向打击（TargetStrike）使用效果：消灭最近的敌人。
     * @param registry ECS 注册表。
     * @param evt      道具使用事件。
     */
    void EffectTargetStrike(Registry& registry, const struct Evt_Item_Use& evt);

    // ── 每帧持续效果更新 ──

    /**
     * @brief 每帧更新全息诱饵状态（计时器倒计时，到期销毁诱饵实体）。
     * @param registry ECS 注册表。
     * @param dt       帧时间（秒）。
     */
    void UpdateHoloBait(Registry& registry, float dt);

    /**
     * @brief 每帧更新 DDoS 冻结状态（计时器倒计时，到期移除 C_D_DDoSFrozen）。
     * @param registry ECS 注册表。
     * @param dt       帧时间（秒）。
     */
    void UpdateDDoSFrozen(Registry& registry, float dt);

    /**
     * @brief 每帧更新流窜 AI 巡逻（随机游走，检测与敌人的碰撞以触发消灭）。
     * @param registry ECS 注册表。
     * @param dt       帧时间（秒）。
     */
    void UpdateRoamAI(Registry& registry, float dt);

    /**
     * @brief 每帧更新光子雷达（刷新计时器到期时重新扫描敌人位置）。
     * @param registry ECS 注册表。
     * @param dt       帧时间（秒）。
     */
    void UpdateRadar(Registry& registry, float dt);

    /**
     * @brief 在所有带 C_T_Enemy + C_D_Transform 的敌人中，找出距 origin 最近的一个。
     * @param registry ECS 注册表。
     * @param origin   参考位置（通常为玩家世界坐标）。
     * @return 最近敌人的 EntityID，若无敌人则返回 Entity::NULL_ENTITY。
     */
    static EntityID FindNearestEnemy(Registry& registry, const NCL::Maths::Vector3& origin);

    /// 流窜 AI 随机游走时使用的简单 PRNG 种子
    uint32_t m_RandSeed = 12345u;

    /**
     * @brief 简单线性同余随机数生成器，返回 [0, 1) 的 float。
     * @return 伪随机浮点数。
     */
    float RandFloat();
};

} // namespace ECS
