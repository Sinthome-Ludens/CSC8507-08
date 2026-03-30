/**
 * @file Res_ChatState.h
 * @brief 聊天/对话系统状态资源：消息队列、回复选项、对话进度
 *
 * @details
 * 跨场景保留（仅回主菜单时清除），对话进度在每次场景 OnAwake 时重置。
 */
#pragma once

#include <cstdint>
#include <cstring>

namespace ECS {

struct ChatMessage {
    char sender[32]  = {};
    char text[128]   = {};
    float timestamp  = 0.0f;
    bool  isSystem   = false;
    uint8_t senderType = 0;  // 0=system, 1=player, 2=NPC
};

enum class DirKey : uint8_t { Up = 0, Down = 1, Left = 2, Right = 3 };

struct DirSequence {
    static constexpr int kDirKeyCount = 4;  ///< DirKey 枚举值数量
    static constexpr int kMaxKeys = 8;
    DirKey  keys[kMaxKeys] = {};
    uint8_t length  = 0;     // 实际长度 3-7
};

struct ChatReplyOption {
    char text[64]    = {};
    bool enabled     = true;
    float alertDelta = 0.0f;  ///< 精确警戒度变化值（正=涨，负=降）
};

struct Res_ChatState {
    static constexpr int   kMaxMessages = 32;
    static constexpr int   kMaxReplies  = 4;
    static constexpr float kPanelWidth  = 320.0f;

    ChatMessage   messages[kMaxMessages] = {};
    int           messageCount           = 0;

    ChatReplyOption replies[kMaxReplies] = {};
    int             replyCount           = 0;
    int8_t          selectedReply        = 0;

    bool chatOpen    = false;
    bool inputActive = false;
    char inputBuf[128] = {};

    // ─ 新增字段 ───────────────────────────────────────────
    float   replyTimer       = 0.0f;
    float   replyTimerMax    = 0.0f;
    bool    replyTimerActive = false;

    uint8_t chatMode         = 0;     // 0=proactive, 1=mixed, 2=passive
    char    currentNodeId[32]= {};    ///< 网状图当前节点 ID（替代旧 dialoguePhase）
    char    forcedTreeId[32] = {};    ///< 非空时强制使用指定 treeId（场景可设置）
    char    pendingAutoNextId[32] = {}; ///< auto-advance 节点的预存跳转目标（"..."回复确认后使用）
    bool    treeFinished     = false; ///< 对话树已正常结束，不再重启
    bool    lockToForcedTree = false;///< true = mode 切换时不启动新树（场景指定了 forcedTreeId）

    float   nextMessageTimer = 0.0f;
    float   nextMessageDelay = 8.0f;
    bool    waitingForReply  = false;

    // ─ 方向键输入系统 ────────────────────────────────────
    static constexpr int kInputBufferSize = 8;
    DirSequence replySequences[kMaxReplies] = {};  // 每个回复的方向序列
    DirKey      inputBuffer[kInputBufferSize] = {};  // 玩家输入缓冲区
    uint8_t     inputBufferLen    = 0;              // 当前输入长度
    bool        dirInputActive    = false;          // 方向输入模式激活

    static_assert(kInputBufferSize >= DirSequence::kMaxKeys,
                  "kInputBufferSize must be >= DirSequence::kMaxKeys");
};

// ─ Free functions (POD compliance) ──────────────────────
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // strncpy deprecation
#endif

/// @brief Append a message to the chat log (drops silently if full).
inline void ChatState_PushMessage(Res_ChatState& cs, const char* sender, const char* text, uint8_t senderType = 0, bool isSystem = false) {
    if (cs.messageCount < Res_ChatState::kMaxMessages) {
        auto& msg = cs.messages[cs.messageCount];
        strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
        msg.sender[sizeof(msg.sender) - 1] = '\0';
        strncpy(msg.text, text, sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        msg.senderType = senderType;
        msg.isSystem   = isSystem;
        msg.timestamp  = 0.0f;
        ++cs.messageCount;
    }
}

/// @brief Reset the player's direction-key input buffer.
inline void ChatState_ClearDirInput(Res_ChatState& cs) {
    cs.inputBufferLen = 0;
    for (auto& k : cs.inputBuffer) k = DirKey::Up;
}

/// @brief Clear all direction sequences and deactivate direction-input mode.
inline void ChatState_ClearDirSequences(Res_ChatState& cs) {
    for (auto& seq : cs.replySequences) { seq.length = 0; }
    ChatState_ClearDirInput(cs);
    cs.dirInputActive = false;
}

/// @brief Clear all reply options, timer, and direction sequences.
inline void ChatState_ClearReplies(Res_ChatState& cs) {
    cs.replyCount    = 0;
    cs.selectedReply = 0;
    for (auto& r : cs.replies) {
        r.text[0] = '\0';
        r.enabled = true;
        r.alertDelta = 0.0f;
    }
    cs.replyTimerActive = false;
    cs.replyTimer       = 0.0f;
    cs.waitingForReply  = false;
    ChatState_ClearDirSequences(cs);
}

/// @brief Add a reply option to the current dialogue node (drops silently if full).
inline void ChatState_AddReply(Res_ChatState& cs, const char* text, float alertDelta = 0.0f) {
    if (cs.replyCount < Res_ChatState::kMaxReplies) {
        auto& r = cs.replies[cs.replyCount];
        strncpy(r.text, text, sizeof(r.text) - 1);
        r.text[sizeof(r.text) - 1] = '\0';
        r.alertDelta = alertDelta;
        r.enabled = true;
        ++cs.replyCount;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace ECS
