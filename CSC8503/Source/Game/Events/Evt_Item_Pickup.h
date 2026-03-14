/**
 * @file Evt_Item_Pickup.h
 * @brief 道具拾取事件：当玩家拾取地图中的道具时发布。
 *
 * @details
 * 由 Sys_Item 在检测到玩家与地图道具实体接触时发布（即时调度）。
 * 监听者：Sys_Item 自身（更新携带数量）、Sys_UI（更新 HUD 显示）。
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_Item.h"

namespace ECS {

/**
 * @brief 道具拾取事件（即时分发）
 *
 * 玩家进入地图道具实体的交互范围并触发拾取操作后发布。
 * 发布前需由 Sys_Item 校验携带数量未达上限，否则不发布。
 */
struct Evt_Item_Pickup {
    EntityID pickerEntity;  ///< 拾取者实体（C_T_Player）
    EntityID pickupEntity;  ///< 被拾取的道具实体（C_T_ItemPickup）
    ItemID   itemId;        ///< 被拾取的道具 ID
};

} // namespace ECS
