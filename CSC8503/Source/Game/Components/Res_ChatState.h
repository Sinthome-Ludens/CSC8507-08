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

struct ChatReplyOption {
    char text[64]   = {};
    bool enabled    = true;
    int8_t effect   = 0;  // -1=bad, 0=neutral, 1=good
};

struct Res_ChatState {
    static constexpr int   kMaxMessages = 32;
    static constexpr int   kMaxReplies  = 4;
    static constexpr float PANEL_WIDTH  = 320.0f;

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
    uint8_t dialoguePhase    = 0;

    float   nextMessageTimer = 0.0f;
    float   nextMessageDelay = 8.0f;
    bool    waitingForReply  = false;

};

// ─ Free functions (POD compliance) ──────────────────────
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // strncpy deprecation
#endif

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

inline void ChatState_ClearReplies(Res_ChatState& cs) {
    cs.replyCount    = 0;
    cs.selectedReply = 0;
    for (auto& r : cs.replies) {
        r.text[0] = '\0';
        r.enabled = true;
        r.effect  = 0;
    }
    cs.replyTimerActive = false;
    cs.replyTimer       = 0.0f;
    cs.waitingForReply  = false;
}

inline void ChatState_AddReply(Res_ChatState& cs, const char* text, int8_t effect = 0) {
    if (cs.replyCount < Res_ChatState::kMaxReplies) {
        auto& r = cs.replies[cs.replyCount];
        strncpy(r.text, text, sizeof(r.text) - 1);
        r.text[sizeof(r.text) - 1] = '\0';
        r.effect  = effect;
        r.enabled = true;
        ++cs.replyCount;
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace ECS
