#pragma once

#include <cstdint>

namespace ECS {

struct ChatMessage {
    char sender[32]  = {};
    char text[128]   = {};
    float timestamp  = 0.0f;
    bool  isSystem   = false;
};

struct ChatReplyOption {
    char text[64] = {};
    bool enabled  = true;
};

struct Res_ChatState {
    static constexpr int kMaxMessages = 32;
    static constexpr int kMaxReplies  = 4;

    ChatMessage   messages[kMaxMessages] = {};
    int           messageCount           = 0;

    ChatReplyOption replies[kMaxReplies] = {};
    int             replyCount           = 0;
    int8_t          selectedReply        = 0;

    bool chatOpen    = false;
    bool inputActive = false;
    char inputBuf[128] = {};
};

} // namespace ECS
