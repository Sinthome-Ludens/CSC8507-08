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

#include <cassert>
#include <cstdint>
#include <cstdio>
#include "Game/Components/C_D_Item.h"
#include "Game/Components/Res_MinimapState.h"

namespace ECS {

/// @brief 单种道具的完整库存记录
struct ItemSlot {
    ItemID  itemId      = ItemID::HoloBait; ///< 道具唯一标识
    ItemType itemType   = ItemType::Gadget; ///< 道具类型
    char    name[24]    = {};               ///< 道具显示名称（UTF-8，如中文最多7字）
    char    desc[80]    = {};               ///< 道具描述文本（UTF-8）

    uint8_t carriedCount = 0;  ///< 本局携带数量
    uint8_t maxCarry     = 2;  ///< 最大携带上限

    uint8_t storeCount   = 0;  ///< 仓库持久存量（跨局）
    uint8_t maxStore     = 99; ///< 仓库最大存量

    uint8_t mapPickupMax = 10; ///< 地图内最多出现数量（光子雷达 = 0）

    float cooldownDuration = 0.0f; ///< 使用后冷却时长（秒），0=无冷却

    bool unlocked = false; ///< 武器解锁状态（持久化），道具始终视为 unlocked

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
struct Res_ItemInventory2 {
    static constexpr int kItemCount = static_cast<int>(ItemID::Count);

    ItemSlot slots[kItemCount] = {};

    /**
     * @brief 构造函数：按需求文档初始化各道具默认参数。
     */
    Res_ItemInventory2() {
        // 001 HoloBait (全息诱饵弹) — Gadget, 始终可用
        slots[0].itemId      = ItemID::HoloBait;
        slots[0].itemType    = ItemType::Gadget;
        slots[0].maxCarry    = 2;
        slots[0].maxStore    = 99;
        slots[0].mapPickupMax = 10;
        slots[0].unlocked    = true;
        slots[0].storeCount  = 5;
        snprintf(slots[0].name, sizeof(slots[0].name), "HoloBait");
        snprintf(slots[0].desc, sizeof(slots[0].desc), "Lure Safe enemies to target");
        slots[0].cooldownDuration = 3.0f;

        // 002 PhotonRadar (光子雷达) — Gadget, 始终可用
        slots[1].itemId      = ItemID::PhotonRadar;
        slots[1].itemType    = ItemType::Gadget;
        slots[1].maxCarry    = 2;
        slots[1].maxStore    = 99;
        slots[1].mapPickupMax = 0;
        slots[1].unlocked    = true;
        slots[1].storeCount  = 3;
        snprintf(slots[1].name, sizeof(slots[1].name), "Radar");
        snprintf(slots[1].desc, sizeof(slots[1].desc), "Reveal enemies on map");
        slots[1].cooldownDuration = 5.0f;

        // 003 DDoS — Gadget, 始终可用
        slots[2].itemId      = ItemID::DDoS;
        slots[2].itemType    = ItemType::Gadget;
        slots[2].maxCarry    = 2;
        slots[2].maxStore    = 99;
        slots[2].mapPickupMax = 10;
        slots[2].unlocked    = true;
        slots[2].storeCount  = 4;
        snprintf(slots[2].name, sizeof(slots[2].name), "DDOS");
        snprintf(slots[2].desc, sizeof(slots[2].desc), "Freeze nearest target 5s");
        slots[2].cooldownDuration = 8.0f;

        // 004 RoamAI (流窜 AI) — Weapon, 默认锁定
        slots[3].itemId      = ItemID::RoamAI;
        slots[3].itemType    = ItemType::Weapon;
        slots[3].maxCarry    = 2;
        slots[3].maxStore    = 0;
        slots[3].storeCount  = 0;
        slots[3].mapPickupMax = 10;
        slots[3].unlocked    = false;
        snprintf(slots[3].name, sizeof(slots[3].name), "RoamAI");
        snprintf(slots[3].desc, sizeof(slots[3].desc), "Patrol AI, kills on contact (AUTO x2)");
        slots[3].cooldownDuration = 5.0f;

        // 005 TargetStrike (靶向打击) — Weapon, 默认锁定
        slots[4].itemId      = ItemID::TargetStrike;
        slots[4].itemType    = ItemType::Weapon;
        slots[4].maxCarry    = 2;
        slots[4].maxStore    = 0;
        slots[4].storeCount  = 0;
        slots[4].mapPickupMax = 10;
        slots[4].unlocked    = false;
        snprintf(slots[4].name, sizeof(slots[4].name), "Strike");
        snprintf(slots[4].desc, sizeof(slots[4].desc), "Kill nearest enemy instantly (AUTO x2)");
        slots[4].cooldownDuration = 10.0f;

        // 006 GlobalMap (全局地图) — Gadget, 始终可用
        slots[5].itemId      = ItemID::GlobalMap;
        slots[5].itemType    = ItemType::Gadget;
        slots[5].maxCarry    = 1;
        slots[5].maxStore    = 99;
        slots[5].mapPickupMax = 2;   // 地图可拾取（稀有，每图最多 2 个）
        slots[5].unlocked    = true;
        slots[5].storeCount  = 3;
        snprintf(slots[5].name, sizeof(slots[5].name), "Map");
        snprintf(slots[5].desc, sizeof(slots[5].desc), "Reveal map layout on HUD");
        slots[5].cooldownDuration = Res_MinimapState::kActiveDuration;
    }

    /**
     * @brief 通过 ItemID 获取对应槽位引用。
     * @param id 道具 ID
     * @return 对应 ItemSlot 的引用
     */
    ItemSlot& Get(ItemID id) {
        int idx = static_cast<int>(id);
        assert(idx >= 0 && idx < kItemCount);
        return slots[idx];
    }

    /**
     * @brief 通过 ItemID 获取对应槽位常量引用。
     * @param id 道具 ID
     * @return 对应 ItemSlot 的常量引用
     */
    const ItemSlot& Get(ItemID id) const {
        int idx = static_cast<int>(id);
        assert(idx >= 0 && idx < kItemCount);
        return slots[idx];
    }

    /**
     * @brief 开局初始化：武器已解锁则自动补满，道具从仓库扣取。
     *
     * @details
     * - Weapon：unlocked ? carriedCount=maxCarry : 0（不消耗 storeCount）
     * - Gadget：carriedCount = min(maxCarry, storeCount)；storeCount -= carriedCount
     */
    void OnRoundStart() {
        for (auto& slot : slots) {
            if (slot.itemType == ItemType::Weapon) {
                slot.carriedCount = slot.unlocked ? slot.maxCarry : 0;
            } else {
                uint8_t take = (slot.storeCount < slot.maxCarry)
                               ? slot.storeCount : slot.maxCarry;
                slot.carriedCount = take;
                slot.storeCount  -= take;
            }
        }
    }

    /**
     * @brief 游戏结束结算：武器清零不回存；道具仅通关时回存仓库。
     *
     * @param isVictory true=通关（gameOverReason==3），剩余道具回存仓库；
     *                  false=死亡/退出，道具丢失。
     */
    void OnRoundEnd(bool isVictory = false) {
        for (auto& slot : slots) {
            if (slot.itemType == ItemType::Weapon) {
                slot.carriedCount = 0;
            } else {
                if (isVictory) {
                    uint16_t sum = static_cast<uint16_t>(slot.storeCount) + slot.carriedCount;
                    slot.storeCount = (sum > slot.maxStore) ? slot.maxStore
                                                           : static_cast<uint8_t>(sum);
                }
                slot.carriedCount = 0;
            }
        }
    }
};

} // namespace ECS
