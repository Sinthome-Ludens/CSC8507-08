#pragma once

#include <cstdint>

/**
 * @brief 玩家选择了一个聊天回复
 *
 * 由 Sys_Chat 发布（当玩家按下 1-4 或点击回复选项时）。
 * 监听者：Sys_Chat 自身处理效果，未来可能 Sys_AI 也监听。
 */
struct Evt_Chat_ReplySelected {
    uint8_t replyIndex;   ///< 选中的回复索引 [0..3]
    uint8_t effectType;   ///< 效果类型: 0=neutral, 1=good, 2=bad
};

/**
 * @brief 回复超时事件
 *
 * 当玩家在限时内未选择回复时由 Sys_Chat 发布。
 * 效果等同于选择了最差回复。
 */
struct Evt_Chat_ReplyTimeout {};
