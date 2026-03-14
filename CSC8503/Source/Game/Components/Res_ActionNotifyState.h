/**
 * @file Res_ActionNotifyState.h
 * @brief 动作通知状态资源：击杀/拾取/奖励等行为的右上角弹出卡片。
 *
 * @details
 * 支持最多 6 条并发通知，循环槽位。格式为"动词 目标名（颜色由类型决定）+分值"。
 * PushActionNotify() 工具函数负责写入此资源并同步更新 Res_GameState.score。
 *
 * @see UI_ActionNotify.h, Res_GameState.h
 */
#pragma once
#include <cstdint>

namespace ECS {

enum class ActionNotifyType : uint8_t {
    Kill       = 0,   ///< 目标名：红色 #C83232
    ItemPickup = 1,   ///< 目标名：绿色 #50B464
    Weapon     = 2,   ///< 目标名：青色 #1EB4C8
    Bonus      = 3,   ///< 目标名：橙色 #FC6F29
    Alert      = 4,   ///< 目标名：琥珀 #C8961E
};

struct ActionNotifyEntry {
    char   verb[32]    = {};          ///< 动词，如 "消灭"、"拾取"（UTF-8：最多 8 个中文字符 = 24 字节，留余量）
    char   target[32]  = {};          ///< 目标名，如 "敌人"、"手枪"
    int    scoreDelta  = 0;           ///< 分值变化，如 +10；0 表示不显示
    float  lifetime    = 2.5f;
    float  elapsed     = 0.0f;
    ActionNotifyType type = ActionNotifyType::Kill;
    bool   active      = false;
};

struct Res_ActionNotifyState {
    static constexpr int kMaxEntries = 6;
    ActionNotifyEntry entries[kMaxEntries] = {};
    int nextSlot = 0;
};

} // namespace ECS
