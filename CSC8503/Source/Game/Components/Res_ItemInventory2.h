/**
 * @file Res_ItemInventory2.h
 * @brief 全局道具库存资源：存储玩家对五种道具的携带数量与仓库存量。
 *
 * @details
 * 场景级 ctx 资源，由 Sys_Item 管理生命周期：
 *  - OnAwake：根据仓库存量初始化携带数量（min(maxCarry, storeCount)）
 *  - 游戏结束时：执行库存结算（storeCount += carriedCount，上限 maxStore）
 *
 * ## 数量限制规则
 * - 携带数量增加：游戏开局从仓库获取 / 游戏中从地图拾取（携带已满则禁止拾取）
 * - 携带数量减少：每次使用道具减 1
 * - 仓库结算增加：游戏结束时 storeCount += carriedCount（上限 maxStore）
 * - 仓库结算减少：每轮开始时 storeCount -= carriedCount
 *
 * @see C_D_Item.h
 * @see Sys_Item.h
 */
#pragma once

#include <cstdint>
#include "Game/Components/C_D_Item.h"

namespace ECS {

/// @brief 单种道具的完整库存记录
struct ItemSlot {
    ItemID  itemId      = ItemID::HoloBait; ///< 道具唯一标识
    ItemType itemType   = ItemType::Gadget; ///< 道具类型

    uint8_t carriedCount = 0;  ///< 本局携带数量
    uint8_t maxCarry     = 2;  ///< 最大携带上限

    uint8_t storeCount   = 0;  ///< 仓库持久存量（跨局）
    uint8_t maxStore     = 99; ///< 仓库最大存量

    uint8_t mapPickupMax = 10; ///< 地图内最多出现数量（光子雷达 = 0）

    /**
     * @brief 检查是否可以从地图拾取（携带未达上限）
     * @return true 表示可拾取
     */
    bool CanPickup() const { return carriedCount < maxCarry; }

    /**
     * @brief 检查是否可以使用（携带数量 > 0）
     * @return true 表示可使用
     */
    bool CanUse() const { return carriedCount > 0; }

    /**
     * @brief 使用一次道具，携带数量减 1。
     * @return true 表示使用成功
     */
    bool UseOne() {
        if (carriedCount == 0) return false;
        --carriedCount;
        return true;
    }

    /**
     * @brief 从地图拾取一次，携带数量加 1（不超过上限）。
     * @return true 表示拾取成功
     */
    bool PickupOne() {
        if (carriedCount >= maxCarry) return false;
        ++carriedCount;
        return true;
    }
};

/**
 * @brief 全局道具库存资源（Scene ctx）
 *
 * 包含五种道具各自的 ItemSlot，通过 ItemID 下标访问。
 */
struct Res_ItemInventory {
    static constexpr int kItemCount = static_cast<int>(ItemID::Count);

    ItemSlot slots[kItemCount] = {};

    /**
     * @brief 构造函数：按需求文档初始化各道具默认参数。
     */
    Res_ItemInventory() {
        // 001 全息诱饵炸弹
        slots[0].itemId      = ItemID::HoloBait;
        slots[0].itemType    = ItemType::Gadget;
        slots[0].maxCarry    = 2;
        slots[0].maxStore    = 99;
        slots[0].mapPickupMax = 10;

        // 002 光子雷达（地图内数量为 0，无商店库存上限视为 99）
        slots[1].itemId      = ItemID::PhotonRadar;
        slots[1].itemType    = ItemType::Gadget;
        slots[1].maxCarry    = 2;
        slots[1].maxStore    = 99;
        slots[1].mapPickupMax = 0;

        // 003 DDoS
        slots[2].itemId      = ItemID::DDoS;
        slots[2].itemType    = ItemType::Gadget;
        slots[2].maxCarry    = 2;
        slots[2].maxStore    = 99;
        slots[2].mapPickupMax = 10;

        // 004 流窜 AI
        slots[3].itemId      = ItemID::RoamAI;
        slots[3].itemType    = ItemType::Weapon;
        slots[3].maxCarry    = 2;
        slots[3].maxStore    = 99;
        slots[3].mapPickupMax = 10;

        // 005 靶向打击
        slots[4].itemId      = ItemID::TargetStrike;
        slots[4].itemType    = ItemType::Weapon;
        slots[4].maxCarry    = 2;
        slots[4].maxStore    = 99;
        slots[4].mapPickupMax = 10;
    }

    /**
     * @brief 通过 ItemID 获取对应槽位引用。
     * @param id 道具 ID
     * @return 对应 ItemSlot 的引用
     */
    ItemSlot& Get(ItemID id) {
        return slots[static_cast<int>(id)];
    }

    /**
     * @brief 通过 ItemID 获取对应槽位常量引用。
     * @param id 道具 ID
     * @return 对应 ItemSlot 的常量引用
     */
    const ItemSlot& Get(ItemID id) const {
        return slots[static_cast<int>(id)];
    }

    /**
     * @brief 开局初始化：从仓库扣除携带数量并设置初始携带。
     *
     * @details
     * 规则：carriedCount = min(maxCarry, storeCount)；
     *       storeCount -= carriedCount。
     */
    void OnRoundStart() {
        for (auto& slot : slots) {
            uint8_t take = (slot.storeCount < slot.maxCarry)
                           ? slot.storeCount : slot.maxCarry;
            slot.carriedCount = take;
            slot.storeCount  -= take;
        }
    }

    /**
     * @brief 游戏结束结算：将剩余携带数量归还仓库（不超过上限）。
     *
     * @details
     * 规则：storeCount = min(maxStore, storeCount + carriedCount)；
     *       carriedCount = 0。
     */
    void OnRoundEnd() {
        for (auto& slot : slots) {
            uint16_t sum = static_cast<uint16_t>(slot.storeCount) + slot.carriedCount;
            slot.storeCount  = (sum > slot.maxStore) ? slot.maxStore : static_cast<uint8_t>(sum);
            slot.carriedCount = 0;
        }
    }
};

} // namespace ECS
