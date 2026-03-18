/**
 * @file ItemEquipSync.cpp
 * @brief MissionSelect 装备 → Res_GameState 显示槽同步实现。
 */
#include "ItemEquipSync.h"

#include <cstring>
#include "Core/ECS/Registry.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Utils/Log.h"

namespace ECS {

void SyncEquipmentToGameState(Registry& registry) {
    if (!registry.has_ctx<Res_UIState>()
     || !registry.has_ctx<Res_GameState>()
     || !registry.has_ctx<Res_ItemInventory2>()) return;

    auto& ui  = registry.ctx<Res_UIState>();
    auto& gs  = registry.ctx<Res_GameState>();
    auto& inv = registry.ctx<Res_ItemInventory2>();

    // Build gadget/weapon index mapping (matches MissionSelect order)
    int gadgetIndices[5] = {};
    int gadgetCount = 0;
    int weaponIndices[5] = {};
    int weaponCount = 0;
    for (int i = 0; i < inv.kItemCount; ++i) {
        if (inv.slots[i].itemType == ItemType::Gadget) {
            if (gadgetCount < 5) gadgetIndices[gadgetCount++] = i;
        } else {
            if (weaponCount < 5) weaponIndices[weaponCount++] = i;
        }
    }

    // Sync item slots
    for (int s = 0; s < 2; ++s) {
        int idx = ui.missionEquippedItems[s];
        if (idx >= 0 && idx < gadgetCount) {
            int invIdx = gadgetIndices[idx];
            auto& slot = inv.slots[invIdx];
            size_t len = strlen(slot.name);
            if (len > sizeof(gs.itemSlots[s].name) - 1)
                len = sizeof(gs.itemSlots[s].name) - 1;
            memcpy(gs.itemSlots[s].name, slot.name, len);
            gs.itemSlots[s].name[len] = '\0';
            gs.itemSlots[s].itemId  = static_cast<uint8_t>(slot.itemId);
            gs.itemSlots[s].count   = slot.carriedCount;
            gs.itemSlots[s].cooldown = 0.0f;
        } else {
            gs.itemSlots[s] = {};
        }
    }

    // Sync weapon slots
    for (int s = 0; s < 2; ++s) {
        int idx = ui.missionEquippedWeapons[s];
        if (idx >= 0 && idx < weaponCount) {
            int invIdx = weaponIndices[idx];
            auto& slot = inv.slots[invIdx];
            size_t len = strlen(slot.name);
            if (len > sizeof(gs.weaponSlots[s].name) - 1)
                len = sizeof(gs.weaponSlots[s].name) - 1;
            memcpy(gs.weaponSlots[s].name, slot.name, len);
            gs.weaponSlots[s].name[len] = '\0';
            gs.weaponSlots[s].itemId  = static_cast<uint8_t>(slot.itemId);
            gs.weaponSlots[s].count   = slot.carriedCount;
            gs.weaponSlots[s].cooldown = 0.0f;
        } else {
            gs.weaponSlots[s] = {};
        }
    }

    LOG_INFO("[ItemEquipSync] Equipment synced: items=["
             << (int)ui.missionEquippedItems[0] << "," << (int)ui.missionEquippedItems[1]
             << "] weapons=[" << (int)ui.missionEquippedWeapons[0] << ","
             << (int)ui.missionEquippedWeapons[1] << "]");
}

} // namespace ECS
