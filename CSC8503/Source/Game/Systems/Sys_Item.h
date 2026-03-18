/**
 * @file Sys_Item.h
 * @brief 道具系统声明（优先级 250）：管理道具的拾取、使用、携带数量及结算。
 *
 * @details
 * Sys_Item 是道具子系统的核心调度者，负责：
 *  1. **拾取检测**：每帧扫描带 C_T_ItemPickup 的实体，检测玩家进入交互范围后
 *     发布 Evt_Item_Pickup（携带已满则跳过）。
 *  2. **使用调度**：监听玩家输入，检查携带数量后发布 Evt_Item_Use，
 *     并递减对应 ItemSlot 的 carriedCount。
 *  3. **结算处理**：在 OnAwake() / OnDestroy() 生命周期内调用 Res_ItemInventory2 的
 *     OnRoundStart() / OnRoundEnd() 以完成开局与结束结算。
 *
 * 系统订阅 Evt_Item_Pickup 以更新 Res_ItemInventory2 中的携带数量，
 * 并在携带数量更新后发布结果供 Sys_UI 刷新 HUD。
 *
 * @see Res_ItemInventory2.h
 * @see Evt_Item_Pickup.h
 * @see Evt_Item_Use.h
 * @see Sys_ItemEffects.h
 */
#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"

namespace ECS {

/**
 * @brief 道具管理系统（优先级 250）
 *
 * 读：C_D_Transform（玩家/道具），C_T_ItemPickup，C_D_Input（使用按键），
 *     Res_ItemInventory2，C_T_Player
 * 写：Res_ItemInventory2（carriedCount），发布 Evt_Item_Pickup / Evt_Item_Use
 */
class Sys_Item : public ISystem {
public:
    /**
     * @brief 系统初始化：初始化 Res_ItemInventory2，订阅拾取事件，执行开局结算。
     * @param registry ECS 注册表。
     */
    void OnAwake(Registry& registry) override;

    /**
     * @brief 每帧更新：拾取范围检测 + 道具使用输入处理。
     * @param registry ECS 注册表。
     * @param dt       帧时间（秒）。
     */
    void OnUpdate(Registry& registry, float dt) override;

    /**
     * @brief 系统销毁：取消事件订阅，执行游戏结束结算。
     * @param registry ECS 注册表。
     */
    void OnDestroy(Registry& registry) override;

private:
    /// 道具拾取事件订阅句柄
    SubscriptionID m_PickupSubId = 0;

    /// 拾取检测半径（玩家中心到道具实体中心）
    static constexpr float kPickupRadius = 2.0f;

    /**
     * @brief 处理拾取事件回调：更新 Res_ItemInventory2 中的携带数量并销毁道具实体。
     * @param registry ECS 注册表（通过 lambda 捕获）。
     * @param evt      拾取事件数据。
     */
    void OnPickup(Registry& registry, const struct Evt_Item_Pickup& evt);

    /**
     * @brief 处理道具使用输入：读取 C_D_Input 中的道具槽按键，发布 Evt_Item_Use。
     * @param registry ECS 注册表。
     */
    void ProcessItemUseInput(Registry& registry);

    /**
     * @brief 每帧扫描地图道具实体，检测玩家是否进入拾取范围。
     * @param registry ECS 注册表。
     */
    void DetectPickup(Registry& registry);

    /// @brief 每帧同步 Res_ItemInventory2 → Res_GameState 显示槽（count/cooldown）。
    void SyncDisplaySlots(Registry& registry, float dt);

    /// @brief 每帧同步 Res_ItemInventory2 → Res_InventoryState（供 UI_Inventory 读取）。
    void SyncInventoryState(Registry& registry);
};

} // namespace ECS
