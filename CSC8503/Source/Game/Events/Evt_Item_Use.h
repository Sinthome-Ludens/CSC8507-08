/**
 * @file Evt_Item_Use.h
 * @brief 道具使用事件：当玩家使用某种道具时发布。
 *
 * @details
 * 由玩家输入处理（Sys_Item）在检测到道具使用按键后发布（即时调度）。
 * 发布前由 Sys_Item 校验携带数量 > 0，并直接调用 UseOne() 等逻辑减少数量；数量不足时不发布该事件。
 *
 * 监听者：
 *  - Sys_ItemEffects：根据 itemId 执行对应效果逻辑
 *  - Sys_UI：刷新 HUD 道具数量显示
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_Item.h"
#include "Vector.h"

namespace ECS {

/**
 * @brief 道具使用事件（即时分发）
 *
 * 玩家按下道具使用键时由 Sys_Item 发布。
 * 携带数量为 0 时不发布该事件（防止空使用）。
 */
struct Evt_Item_Use {
    EntityID          userEntity;    ///< 使用者实体（C_T_Player）
    EntityID          targetEntity;  ///< 目标实体（可以与使用者相同；无目标时为 Entity::NULL_ENTITY）
    ItemID            itemId;        ///< 使用的道具 ID
    NCL::Maths::Vector3 targetPos;  ///< 投掷/作用目标世界坐标（HoloBait/DDoS 用）
    bool              isOnline;      ///< 是否为联机模式（影响部分道具效果）
};

} // namespace ECS
