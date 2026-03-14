/**
 * @file C_T_ItemPickup.h
 * @brief 地图道具拾取标签组件：标记实体为地图中可拾取的道具实体。
 *
 * @details
 * 挂载此组件的实体表示地图上的一个可拾取道具。
 * Sys_Item 每帧扫描带此标签的实体，通过玩家与道具在 XZ 平面的距离检测接近，
 * 并在满足拾取条件时发布 Evt_Item_Pickup。
 *
 * 组合规范：
 *  挂载此组件的实体通常还会包含：
 *   - C_D_Transform（世界坐标，用于距离检测）
 *   - C_D_MeshRenderer（占位渲染，用于在场景中可视化道具）
 *  当前实现的拾取逻辑基于距离检测，不依赖物理 Trigger Collider。
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
 * 标记实体为可拾取道具，并记录道具类型及单次拾取给予的数量。
 */
struct C_T_ItemPickup {
    ItemID  itemId   = ItemID::HoloBait; ///< 该实体代表的道具 ID
    uint8_t quantity = 1;                ///< 触发拾取事件时给予玩家的道具数量（通常为 1）
};

} // namespace ECS
