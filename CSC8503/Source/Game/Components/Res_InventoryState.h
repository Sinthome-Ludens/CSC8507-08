#pragma once

#include <cstdint>
#include <cstdio>

namespace ECS {

/**
 * @brief 背包/物品栏全局状态资源
 *
 * 存储玩家背包中所有物品的显示信息。
 * 由 Sys_Inventory（未来）写入，由 UI_Inventory 和 UI_ItemWheel 读取渲染。
 *
 * @note 作为 Registry Context 资源，不受 64 字节限制，但保持 POD 特性。
 */
struct Res_InventoryState {
    struct Slot {
        char    name[16] = {};
        uint8_t type     = 0;   // 0=empty, 1=item, 2=weapon
        int8_t  count    = 0;
        char    desc[32] = {};
    };
    static constexpr int MAX_SLOTS = 12;
    Slot slots[MAX_SLOTS] = {};
    int8_t selectedSlot = 0;

    /// 查找第一个空槽位，返回索引，-1表示已满
    int FindEmptySlot() const {
        for (int i = 0; i < MAX_SLOTS; ++i) {
            if (slots[i].type == 0) return i;
        }
        return -1;
    }

    /// 添加物品到第一个空槽位
    bool AddItem(const char* name, uint8_t type, int8_t count, const char* desc) {
        int idx = FindEmptySlot();
        if (idx < 0) return false;
        auto& s = slots[idx];
        snprintf(s.name, sizeof(s.name), "%s", name);
        s.type = type;
        s.count = count;
        snprintf(s.desc, sizeof(s.desc), "%s", desc);
        return true;
    }
};

} // namespace ECS
