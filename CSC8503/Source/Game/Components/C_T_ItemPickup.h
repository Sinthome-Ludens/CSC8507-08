/**
 * @file C_T_ItemPickup.h
 * @brief 地图道具拾取标签组件：标记实体为地图中可拾取的道具实体。
 *
 * @details
 * 挂载此组件的实体表示地图上的一个可拾取道具。
 * Sys_Item 每帧扫描带此标签的实体，检测玩家接近时发布 Evt_Item_Pickup。
 *
 * 组合规范：
 *  挂载此组件的实体同时需要：
 *   - C_D_Transform（世界坐标）
 *   - C_D_MeshRenderer（占位渲染）
 *   - C_D_Collider（is_trigger=true, 用于检测玩家进入范围）
 *
 * @see Evt_Item_Pickup.h
 * @see Sys_Item.h
 */
#pragma once

#include <cstdint>
#include "Game/Components/C_D_Item.h"

namespace ECS {

/**
 * @brief 地图道具实体标签组件
 *
 * 标记实体为可拾取道具，并记录道具类型与剩余数量。
 * 当 quantity 归零时，由 Sys_Item 销毁该实体（延迟销毁）。
 */
struct C_T_ItemPickup {
    ItemID  itemId   = ItemID::HoloBait; ///< 该实体代表的道具 ID
    uint8_t quantity = 1;                ///< 拾取后给予玩家的数量（通常为 1）
};

} // namespace ECS
