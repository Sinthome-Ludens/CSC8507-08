/**
 * @file Res_InventoryState.h
 * @brief 物品栏状态资源：3x4 格子、选中槽位、详情面板
 *
 * @details
 * Scene 级 ctx 资源，场景切换时在 OnExit 中清除。
 */
#pragma once

#include <cstdint>

namespace ECS {

struct InventoryItem {
    char     name[32]        = {};
    char     description[64] = {};
    uint8_t  quantity         = 0;
    uint8_t  iconIndex        = 0;
    bool     isEmpty          = true;
};

struct Res_InventoryState {
    static constexpr int kRows = 3;
    static constexpr int kCols = 4;
    static constexpr int kSlotCount = kRows * kCols;

    InventoryItem slots[kSlotCount] = {};
    int8_t        selectedSlot      = 0;
    bool          detailPanelOpen   = false;
};

} // namespace ECS
