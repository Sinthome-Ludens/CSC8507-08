#include "Sys_Chat.h"

#include "Window.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Utils/Log.h"

#include <algorithm>
#include <cstring>

using namespace NCL;

namespace ECS {

// ============================================================
// Dialogue tree data (file-scope static arrays)
// ============================================================

struct DialogueNode {
    const char* npcMessage;
    const char* replies[4];
    int8_t      effects[4];   // -1=bad, 0=neutral, 1=good
    int          replyCount;
    float        replyTimeLimit;  // 0 = no limit
};

// ── Proactive dialogue (chatMode 0): 3 nodes, no time limit ──
static const DialogueNode kProactiveDialogue[] = {
    {
        "All clear ahead. Ready to proceed?",
        { "Copy that, moving in.", "Hold position.", "What's the situation?", "Let me scout first." },
        { 1, 0, 0, 1 },
        4, 0.0f
    },
    {
        "Intel suggests minimal resistance in the next sector.",
        { "Good, stay sharp.", "Understood.", "Any alternate routes?", "I don't trust that intel." },
        { 1, 0, 0, -1 },
        4, 0.0f
    },
    {
        "We're making good progress. Extraction ready on your signal.",
        { "Acknowledged. Continuing.", "Keep the line open.", "How's our timeline?", "Almost done here." },
        { 1, 0, 0, 1 },
        4, 0.0f
    },
};
static constexpr int kProactiveCount = 3;

// ── Mixed dialogue (chatMode 1): 2 nodes, 10-12s limit ──
static const DialogueNode kMixedDialogue[] = {
    {
        "Movement detected nearby. What's your call?",
        { "Go silent.", "Push through.", "Find cover.", "Abort route." },
        { 1, -1, 1, 0 },
        4, 12.0f
    },
    {
        "Comms might be compromised. Keep it brief.",
        { "Roger. Eyes open.", "Switching frequency.", "How compromised?" },
        { 1, 0, -1 },
        3, 10.0f
    },
};
static constexpr int kMixedCount = 2;

// ── Passive dialogue (chatMode 2): 1 node, 6s limit ──
static const DialogueNode kPassiveDialogue[] = {
    {
        "You've been spotted! Respond NOW!",
        { "Engaging.", "Evading!", "Need backup!" },
        { -1, 1, 0 },
        3, 6.0f
    },
};
static constexpr int kPassiveCount = 1;

// ============================================================
// Helper: get dialogue node for current mode and phase
// ============================================================
static const DialogueNode* GetCurrentNode(uint8_t chatMode, uint8_t phase) {
    switch (chatMode) {
        case 0:
            if (phase < kProactiveCount) return &kProactiveDialogue[phase];
            break;
        case 1:
            if (phase < kMixedCount) return &kMixedDialogue[phase];
            break;
        case 2:
            if (phase < kPassiveCount) return &kPassiveDialogue[phase];
            break;
    }
    return nullptr;
}

static int GetMaxPhase(uint8_t chatMode) {
    switch (chatMode) {
        case 0: return kProactiveCount;
        case 1: return kMixedCount;
        case 2: return kPassiveCount;
        default: return 0;
    }
}

// ============================================================
// OnAwake: initialize chat state with system messages
// ============================================================

void Sys_Chat::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_ChatState>()) return;
    auto& chat = registry.ctx<Res_ChatState>();

    ChatState_PushMessage(chat,"SYSTEM", "COMMS LINK ESTABLISHED", 0, true);
    ChatState_PushMessage(chat,"SYSTEM", "Awaiting operator input...", 0, true);

    // Set initial NPC message delay
    chat.nextMessageDelay = 8.0f;
    chat.nextMessageTimer = 3.0f;  // First NPC message after 3s
    chat.dialoguePhase    = 0;

    LOG_INFO("[Sys_Chat] OnAwake — Chat initialized with 2 system messages");
}

// ============================================================
// OnUpdate
// ============================================================

void Sys_Chat::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_ChatState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;

    auto& chat = registry.ctx<Res_ChatState>();
    auto& ui   = registry.ctx<Res_UIState>();

    // Only run during HUD
    if (ui.activeScreen != UIScreen::HUD) return;

    // Get game state for alert-based mode switching
    float alertLevel = 0.0f;
    if (registry.has_ctx<Res_GameState>()) {
        alertLevel = registry.ctx<Res_GameState>().alertLevel;
    }

    // ── Update chatMode based on alertLevel ───────────────
    uint8_t newMode;
    if (alertLevel <= 30.0f) {
        newMode = 0;  // proactive
    } else if (alertLevel <= 50.0f) {
        newMode = 1;  // mixed
    } else {
        newMode = 2;  // passive
    }

    if (newMode != chat.chatMode) {
        chat.chatMode      = newMode;
        chat.dialoguePhase = 0;
        ChatState_ClearReplies(chat);

        // Adjust NPC message delay by mode
        switch (chat.chatMode) {
            case 0: chat.nextMessageDelay = 8.0f; break;
            case 1: chat.nextMessageDelay = 5.0f; break;
            case 2: chat.nextMessageDelay = 3.0f; break;
        }

        chat.nextMessageTimer = 1.0f;  // Quick re-trigger on mode change
        LOG_INFO("[Sys_Chat] Mode changed to " << (int)chat.chatMode);
    }

    // ── Handle keyboard input for replies ─────────────────
    const Keyboard* kb = Window::GetKeyboard();
    int8_t confirmedReply = -1;

    if (kb && chat.replyCount > 0) {
        // W/S navigate
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            chat.selectedReply = (chat.selectedReply - 1 + chat.replyCount) % chat.replyCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            chat.selectedReply = (chat.selectedReply + 1) % chat.replyCount;
        }

        // Enter to confirm
        if (kb->KeyPressed(KeyCodes::RETURN)) {
            confirmedReply = chat.selectedReply;
        }

        // 1-4 direct select + confirm
        if (kb->KeyPressed(KeyCodes::NUM1) && chat.replyCount > 0) confirmedReply = 0;
        if (kb->KeyPressed(KeyCodes::NUM2) && chat.replyCount > 1) confirmedReply = 1;
        if (kb->KeyPressed(KeyCodes::NUM3) && chat.replyCount > 2) confirmedReply = 2;
        if (kb->KeyPressed(KeyCodes::NUM4) && chat.replyCount > 3) confirmedReply = 3;
    }

    // ── Process confirmed reply ───────────────────────────
    if (confirmedReply >= 0 && confirmedReply < chat.replyCount) {
        auto& reply = chat.replies[confirmedReply];

        // Push player message
        ChatState_PushMessage(chat,"YOU", reply.text, 1, false);

        // Apply effect to alertLevel
        if (registry.has_ctx<Res_GameState>()) {
            auto& gs = registry.ctx<Res_GameState>();
            if (reply.effect > 0) {
                gs.alertLevel = std::max(0.0f, gs.alertLevel - 5.0f);
                LOG_INFO("[Sys_Chat] Good reply — alert -5");
            } else if (reply.effect < 0) {
                gs.alertLevel = std::min(gs.alertMax, gs.alertLevel + 10.0f);
                LOG_INFO("[Sys_Chat] Bad reply — alert +10");
            }
        }

        // Advance dialogue phase
        chat.dialoguePhase++;
        ChatState_ClearReplies(chat);
        chat.nextMessageTimer = 2.0f;  // Next NPC message after 2s
        chat.waitingForReply  = false;
    }

    // ── Reply timer (timeout) ─────────────────────────────
    if (chat.replyTimerActive && chat.replyCount > 0) {
        chat.replyTimer -= dt;
        if (chat.replyTimer <= 0.0f) {
            // Timeout penalty
            ChatState_PushMessage(chat,"SYSTEM", "Response timeout — alert increased", 0, true);
            if (registry.has_ctx<Res_GameState>()) {
                auto& gs = registry.ctx<Res_GameState>();
                gs.alertLevel = std::min(gs.alertMax, gs.alertLevel + 15.0f);
            }
            chat.dialoguePhase++;
            ChatState_ClearReplies(chat);
            chat.nextMessageTimer = 2.0f;
            LOG_INFO("[Sys_Chat] Reply timeout — alert +15");
        }
    }

    // ── NPC message scheduling ────────────────────────────
    if (!chat.waitingForReply && chat.replyCount == 0) {
        chat.nextMessageTimer -= dt;
        if (chat.nextMessageTimer <= 0.0f) {
            // Get current dialogue node
            const DialogueNode* node = GetCurrentNode(chat.chatMode, chat.dialoguePhase);
            if (node) {
                // Push NPC message
                ChatState_PushMessage(chat,"HANDLER", node->npcMessage, 2, false);

                // Set up reply options
                for (int i = 0; i < node->replyCount; ++i) {
                    ChatState_AddReply(chat,node->replies[i], node->effects[i]);
                }
                chat.selectedReply = 0;

                // Set timer if applicable
                if (node->replyTimeLimit > 0.0f) {
                    chat.replyTimerActive = true;
                    chat.replyTimer       = node->replyTimeLimit;
                    chat.replyTimerMax    = node->replyTimeLimit;
                }

                chat.waitingForReply = true;
            } else {
                // No more dialogue nodes — wrap around
                chat.dialoguePhase = 0;
                chat.nextMessageTimer = chat.nextMessageDelay;
            }
        }
    }
}

} // namespace ECS
