/**
 * @file ItemEquipSync.h
 * @brief MissionSelect 装备 → Res_GameState 显示槽同步（一次性，场景 OnEnter 调用）。
 */
#pragma once

namespace ECS {
class Registry;

/// @brief 从 Res_UIState.missionEquippedItems/Weapons 同步到 Res_GameState.itemSlots/weaponSlots。
void SyncEquipmentToGameState(Registry& registry);

/// @brief 从 Res_ItemInventory2 中自动填充空的 HUD 槽位（carriedCount>0 的道具按序填入）。
void AutoFillHUDSlots(Registry& registry);

} // namespace ECS
