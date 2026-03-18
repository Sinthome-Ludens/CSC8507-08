/**
 * @file Res_DialogueData.h
 * @brief 对话数据资源：3 模式网状对话图（从 JSON 加载）
 *
 * 每个节点属性：id, speaker, message, alertDelta, timeLimit
 * 每个回复属性：text, alertDelta, nextNodeId
 * JSON 中的 nextSpeaker / comment 仅供策划标注，不加载到 C++ 结构体。
 */
#pragma once

#include <cstdint>
#include <cstring>

namespace ECS {

/// 单个对话节点（POD）— 网状图节点，通过 id/nextNodeId 互相引用
struct DialogueNode {
    char    id[32]              = {};       ///< 唯一节点 ID（如 "p_01"）
    char    speaker[32]         = {};       ///< 本语句角色名（如 "HANDLER"）
    char    npcMessage[128]     = {};       ///< 对话内容
    float   alertDelta          = 0.0f;     ///< 节点显示时的全局警戒度变化

    char    replies[4][64]      = {};       ///< 回复选项文本
    float   replyAlertDelta[4]  = {};       ///< 每个回复的精确警戒度变化值
    char    nextNodeId[4][32]   = {};       ///< 每个回复跳转的目标节点 ID，"" = 对话结束

    uint8_t replyCount          = 0;
    float   replyTimeLimit      = 0.0f;     ///< 0 = 无时限

    bool    waitReply           = true;     ///< true=等待玩家回复, false=自动推进
    char    autoNextId[32]      = {};       ///< waitReply=false 时的自动跳转目标，"" = 对话结束
    bool    isLoop              = false;    ///< true=回复后循环回本节点（持续对话）
};

/// 对话树入口信息（treeId 用于指定/随机选择，rootNodeId 为入口节点）
struct DialogueTreeInfo {
    char treeId[32]     = {};   ///< 对话树唯一 ID（如 "delivery", "repair"）
    char rootNodeId[32] = {};   ///< 入口节点 ID（如 "na_01"）
};

/// 单个对话模式的网状图（最多 64 个节点，可含多棵对话树）
struct DialogueSequence {
    static constexpr int kMaxNodes = 128;
    static constexpr int kMaxTrees = 8;     ///< 同一文件中最多 8 棵对话树
    DialogueNode     nodes[kMaxNodes]  = {};
    int              nodeCount         = 0;
    float            messageDelay      = 8.0f;   ///< NPC 消息间隔（秒）
    DialogueTreeInfo trees[kMaxTrees]  = {};     ///< 所有对话树入口
    int              treeCount         = 0;
};

/// Registry context resource — 对话数据（3 个模式各一组网状对话图）
struct Res_DialogueData {
    DialogueSequence proactive;    ///< chatMode 0: alertLevel 0-30
    DialogueSequence mixed;        ///< chatMode 1: alertLevel 31-50
    DialogueSequence passive;      ///< chatMode 2: alertLevel 51+
    bool loaded = false;           ///< 数据是否已从 JSON 加载
};

} // namespace ECS
