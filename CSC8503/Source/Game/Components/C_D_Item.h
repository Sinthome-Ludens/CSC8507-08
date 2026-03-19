/**
 * @file C_D_Item.h
 * @brief 道具数据组件：存储单种道具的 ID、类型、携带数量及库存数量。
 *
 * @details
 * 每个持有道具的实体（玩家）应为每种道具各挂载一个 C_D_Item，
 * 或通过 Res_ItemInventory2 统一管理全局库存。
 *
 * ## 道具编号对应关系
 * | ItemID           | 名称         | 类型   |
 * |------------------|--------------|--------|
 * | HoloBait         | 全息诱饵炸弹 | Gadget |
 * | PhotonRadar      | 光子雷达     | Gadget |
 * | DDoS             | DDoS         | Gadget |
 * | RoamAI           | 流窜 AI      | Weapon |
 * | TargetStrike     | 靶向打击     | Weapon |
 * | GlobalMap        | 全局地图     | Gadget |
 *
 * @see Res_ItemInventory2.h
 * @see Sys_Item.h
 */
#pragma once

#include <cstdint>

namespace ECS {

/// @brief 道具类型枚举
enum class ItemType : uint8_t {
    Gadget = 0, ///< 道具（不直接造成伤害，具有特殊效果）
    Weapon = 1, ///< 武器（对目标造成生命值伤害）
};

/// @brief 道具唯一 ID 枚举（对应需求文档 001~005）
enum class ItemID : uint8_t {
    HoloBait     = 0, ///< 001 全息诱饵炸弹 — 使敌人移动到投掷位置
    PhotonRadar  = 1, ///< 002 光子雷达     — 在 UI 上显示敌人位置
    DDoS         = 2, ///< 003 DDoS         — 冻结目标 5 秒
    RoamAI       = 3, ///< 004 流窜 AI      — 释放巡逻 AI，触碰敌人即消灭
    TargetStrike = 4, ///< 005 靶向打击     — 击毙目标（直接死亡）
    GlobalMap    = 5, ///< 006 全局地图     — 使用后显示持续小地图
    Count        = 6, ///< 道具种类总数（用于数组索引）
};

/**
 * @brief 道具数据组件
 * @deprecated 此结构体目前未被任何实体挂载，库存管理已迁移至 Res_ItemInventory2。
 *             保留仅供未来多人模式中每实体道具追踪扩展。
 *
 * 每个 ItemID 对应一条独立的 C_D_Item 记录，挂载在道具拾取实体上，
 * 或在玩家实体上通过携带数量追踪。
 *
 * 生命周期规则：
 *  - carriedCount 增加：开局从库存获取 / 地图内拾取
 *  - carriedCount 减少：每次使用减 1
 *  - storeCount 结算：游戏结束时 storeCount += carriedCount（不超过 maxStore）
 *  - storeCount 开局：storeCount -= maxCarry（不低于 0）
 */
struct C_D_Item {
    ItemID   itemId       = ItemID::HoloBait; ///< 道具唯一标识
    ItemType itemType     = ItemType::Gadget; ///< 道具类型（Gadget / Weapon）

    uint8_t  carriedCount = 0;               ///< 当前携带数量
    uint8_t  maxCarry     = 2;               ///< 最大携带数量（开局最多 2 个）

    uint8_t  storeCount   = 0;               ///< 玩家仓库存量（跨局持久化）
    uint8_t  maxStore     = 99;              ///< 仓库最大存量（光子雷达无上限视为 99）
};

/// @brief 根据 ItemID 查询对应的 ItemType（集中维护，避免多处硬编码）。
inline ItemType GetItemType(ItemID id) {
    switch (id) {
        case ItemID::RoamAI:       return ItemType::Weapon;
        case ItemID::TargetStrike: return ItemType::Weapon;
        default:                   return ItemType::Gadget;
    }
}

} // namespace ECS
