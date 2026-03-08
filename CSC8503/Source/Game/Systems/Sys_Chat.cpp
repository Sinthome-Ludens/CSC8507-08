#include "Sys_Chat.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // strncpy deprecation
#endif

#include "Window.h"
#include "Assets.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/Utils/DialogueLoader.h"
#include "Game/Utils/Log.h"

#include <algorithm>
#include <cstring>

using namespace NCL;

namespace ECS {

// ============================================================
// Helper: get dialogue sequence for current mode
// ============================================================
static const DialogueSequence* GetSequence(const Res_DialogueData& data, uint8_t chatMode) {
    switch (chatMode) {
        case 0: return &data.proactive;
        case 1: return &data.mixed;
        case 2: return &data.passive;
        default: return nullptr;
    }
}

// ============================================================
// Fallback hardcoded data (used if JSON loading fails)
// ============================================================
static void LoadFallbackDialogue(Res_DialogueData& data) {
    // Proactive — 3 nodes
    {
        auto& seq = data.proactive;
        seq.messageDelay = 8.0f;
        seq.nodeCount = 3;

        auto& n0 = seq.nodes[0];
        strncpy(n0.npcMessage, "All clear ahead. Ready to proceed?", sizeof(n0.npcMessage) - 1);
        strncpy(n0.replies[0], "Copy that, moving in.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Hold position.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[2], "What's the situation?", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[3], "Let me scout first.", sizeof(n0.replies[0]) - 1);
        n0.effects[0] = 1; n0.effects[1] = 0; n0.effects[2] = 0; n0.effects[3] = 1;
        n0.replyCount = 4; n0.replyTimeLimit = 0.0f;

        auto& n1 = seq.nodes[1];
        strncpy(n1.npcMessage, "Intel suggests minimal resistance in the next sector.", sizeof(n1.npcMessage) - 1);
        strncpy(n1.replies[0], "Good, stay sharp.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[1], "Understood.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[2], "Any alternate routes?", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[3], "I don't trust that intel.", sizeof(n1.replies[0]) - 1);
        n1.effects[0] = 1; n1.effects[1] = 0; n1.effects[2] = 0; n1.effects[3] = -1;
        n1.replyCount = 4; n1.replyTimeLimit = 0.0f;

        auto& n2 = seq.nodes[2];
        strncpy(n2.npcMessage, "We're making good progress. Extraction ready on your signal.", sizeof(n2.npcMessage) - 1);
        strncpy(n2.replies[0], "Acknowledged. Continuing.", sizeof(n2.replies[0]) - 1);
        strncpy(n2.replies[1], "Keep the line open.", sizeof(n2.replies[0]) - 1);
        strncpy(n2.replies[2], "How's our timeline?", sizeof(n2.replies[0]) - 1);
        strncpy(n2.replies[3], "Almost done here.", sizeof(n2.replies[0]) - 1);
        n2.effects[0] = 1; n2.effects[1] = 0; n2.effects[2] = 0; n2.effects[3] = 1;
        n2.replyCount = 4; n2.replyTimeLimit = 0.0f;
    }

    // Mixed — 2 nodes
    {
        auto& seq = data.mixed;
        seq.messageDelay = 5.0f;
        seq.nodeCount = 2;

        auto& n0 = seq.nodes[0];
        strncpy(n0.npcMessage, "Movement detected nearby. What's your call?", sizeof(n0.npcMessage) - 1);
        strncpy(n0.replies[0], "Go silent.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Push through.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[2], "Find cover.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[3], "Abort route.", sizeof(n0.replies[0]) - 1);
        n0.effects[0] = 1; n0.effects[1] = -1; n0.effects[2] = 1; n0.effects[3] = 0;
        n0.replyCount = 4; n0.replyTimeLimit = 12.0f;

        auto& n1 = seq.nodes[1];
        strncpy(n1.npcMessage, "Comms might be compromised. Keep it brief.", sizeof(n1.npcMessage) - 1);
        strncpy(n1.replies[0], "Roger. Eyes open.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[1], "Switching frequency.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[2], "How compromised?", sizeof(n1.replies[0]) - 1);
        n1.effects[0] = 1; n1.effects[1] = 0; n1.effects[2] = -1;
        n1.replyCount = 3; n1.replyTimeLimit = 10.0f;
    }

    // Passive — 1 node
    {
        auto& seq = data.passive;
        seq.messageDelay = 3.0f;
        seq.nodeCount = 1;

        auto& n0 = seq.nodes[0];
        strncpy(n0.npcMessage, "You've been spotted! Respond NOW!", sizeof(n0.npcMessage) - 1);
        strncpy(n0.replies[0], "Engaging.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Evading!", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[2], "Need backup!", sizeof(n0.replies[0]) - 1);
        n0.effects[0] = -1; n0.effects[1] = 1; n0.effects[2] = 0;
        n0.replyCount = 3; n0.replyTimeLimit = 6.0f;
    }

    data.loaded = true;
}

// ============================================================
// OnAwake
// ============================================================

void Sys_Chat::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_ChatState>()) return;
    auto& chat = registry.ctx<Res_ChatState>();

    ChatState_PushMessage(chat, "SYSTEM", "COMMS LINK ESTABLISHED", 0, true);
    ChatState_PushMessage(chat, "SYSTEM", "Awaiting operator input...", 0, true);

    chat.nextMessageDelay = 8.0f;
    chat.nextMessageTimer = 3.0f;
    chat.dialoguePhase    = 0;

    // Load dialogue data from JSON
    if (!registry.has_ctx<Res_DialogueData>()) {
        registry.ctx_emplace<Res_DialogueData>();
    }
    auto& dialogueData = registry.ctx<Res_DialogueData>();

    if (!dialogueData.loaded) {
        std::string path = NCL::Assets::DATADIR + "Dialogue_Default.json";
        if (!LoadDialogueFromJSON(path, dialogueData)) {
            LOG_WARN("[Sys_Chat] JSON load failed, using fallback dialogue");
            LoadFallbackDialogue(dialogueData);
        }
    }

    LOG_INFO("[Sys_Chat] OnAwake — Chat initialized, dialogue data "
             << (dialogueData.loaded ? "loaded" : "MISSING"));
}

// ============================================================
// OnUpdate
// ============================================================

void Sys_Chat::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_ChatState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_DialogueData>()) return;

    auto& chat = registry.ctx<Res_ChatState>();
    auto& ui   = registry.ctx<Res_UIState>();
    const auto& dialogueData = registry.ctx<Res_DialogueData>();

    if (ui.activeScreen != UIScreen::HUD) return;
    if (!dialogueData.loaded) return;

    // Get alert level for mode switching
    float alertLevel = 0.0f;
    if (registry.has_ctx<Res_GameState>()) {
        alertLevel = registry.ctx<Res_GameState>().alertLevel;
    }

    // ── Update chatMode based on alertLevel ───────────────
    uint8_t newMode;
    if (alertLevel <= 30.0f) {
        newMode = 0;
    } else if (alertLevel <= 50.0f) {
        newMode = 1;
    } else {
        newMode = 2;
    }

    if (newMode != chat.chatMode) {
        chat.chatMode      = newMode;
        chat.dialoguePhase = 0;
        ChatState_ClearReplies(chat);

        const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
        chat.nextMessageDelay = seq ? seq->messageDelay : 8.0f;
        chat.nextMessageTimer = 1.0f;
        LOG_INFO("[Sys_Chat] Mode changed to " << (int)chat.chatMode);
    }

    // ── Handle keyboard input for replies ─────────────────
    const Keyboard* kb = Window::GetKeyboard();
    int8_t confirmedReply = -1;

    if (kb && chat.replyCount > 0) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            chat.selectedReply = (chat.selectedReply - 1 + chat.replyCount) % chat.replyCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            chat.selectedReply = (chat.selectedReply + 1) % chat.replyCount;
        }
        if (kb->KeyPressed(KeyCodes::RETURN)) {
            confirmedReply = chat.selectedReply;
        }
        if (kb->KeyPressed(KeyCodes::NUM1) && chat.replyCount > 0) confirmedReply = 0;
        if (kb->KeyPressed(KeyCodes::NUM2) && chat.replyCount > 1) confirmedReply = 1;
        if (kb->KeyPressed(KeyCodes::NUM3) && chat.replyCount > 2) confirmedReply = 2;
        if (kb->KeyPressed(KeyCodes::NUM4) && chat.replyCount > 3) confirmedReply = 3;
    }

    // ── Process confirmed reply ───────────────────────────
    if (confirmedReply >= 0 && confirmedReply < chat.replyCount) {
        auto& reply = chat.replies[confirmedReply];

        ChatState_PushMessage(chat, "YOU", reply.text, 1, false);

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

        chat.dialoguePhase++;
        ChatState_ClearReplies(chat);
        chat.nextMessageTimer = 2.0f;
        chat.waitingForReply  = false;
    }

    // ── Reply timer (timeout) ─────────────────────────────
    if (chat.replyTimerActive && chat.replyCount > 0) {
        chat.replyTimer -= dt;
        if (chat.replyTimer <= 0.0f) {
            ChatState_PushMessage(chat, "SYSTEM", "Response timeout — alert increased", 0, true);
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

    // ── NPC message scheduling (data-driven) ──────────────
    if (!chat.waitingForReply && chat.replyCount == 0) {
        chat.nextMessageTimer -= dt;
        if (chat.nextMessageTimer <= 0.0f) {
            const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
            if (seq && chat.dialoguePhase < seq->nodeCount) {
                const auto& node = seq->nodes[chat.dialoguePhase];

                ChatState_PushMessage(chat, "HANDLER", node.npcMessage, 2, false);

                for (int i = 0; i < node.replyCount; ++i) {
                    ChatState_AddReply(chat, node.replies[i], node.effects[i]);
                }
                chat.selectedReply = 0;

                if (node.replyTimeLimit > 0.0f) {
                    chat.replyTimerActive = true;
                    chat.replyTimer       = node.replyTimeLimit;
                    chat.replyTimerMax    = node.replyTimeLimit;
                }

                chat.waitingForReply = true;
            } else {
                // Wrap around
                chat.dialoguePhase = 0;
                chat.nextMessageTimer = seq ? seq->messageDelay : chat.nextMessageDelay;
            }
        }
    }
}

} // namespace ECS

#ifdef _MSC_VER
#pragma warning(pop)
#endif
