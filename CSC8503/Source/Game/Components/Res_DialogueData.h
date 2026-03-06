#pragma once

#include <cstdint>

namespace ECS {

/// 单个对话节点（POD）
struct DialogueNode {
    char    npcMessage[128]  = {};
    char    replies[4][64]   = {};
    int8_t  effects[4]       = {};        // -1=bad, 0=neutral, 1=good
    uint8_t replyCount       = 0;
    float   replyTimeLimit   = 0.0f;      // 0 = 无时限
};

/// 单个对话模式的节点集合（最多 16 个节点）
struct DialogueSequence {
    static constexpr int kMaxNodes = 16;
    DialogueNode nodes[kMaxNodes] = {};
    int          nodeCount        = 0;
    float        messageDelay     = 8.0f;  // NPC 消息间隔（秒）
};

/// Registry context resource — 对话数据（3 个模式各一组对话序列）
struct Res_DialogueData {
    DialogueSequence proactive;    ///< chatMode 0: alertLevel 0-30
    DialogueSequence mixed;        ///< chatMode 1: alertLevel 31-50
    DialogueSequence passive;      ///< chatMode 2: alertLevel 51+
    bool loaded = false;           ///< 数据是否已从 JSON 加载
};

} // namespace ECS
