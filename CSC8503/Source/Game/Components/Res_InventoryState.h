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
