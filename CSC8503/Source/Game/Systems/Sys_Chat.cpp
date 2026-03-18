/**
 * @file Sys_Chat.cpp
 * @brief 聊天/对话系统实现。
 *
 * - OnAwake: 加载对话 JSON 并初始化 Res_ChatState / Res_DialogueData。
 * - OnUpdate: 驱动对话流程（节点推进 / 方向键输入 / 倒计时 / 回复确认）。
 */
#include "Sys_Chat.h"
#include "Game/Utils/PauseGuard.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // strncpy deprecation
#endif

#include "Window.h"
#include "Assets.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/StratagemTable.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/Utils/DialogueLoader.h"
#include "Game/Utils/Log.h"

#include <algorithm>
#include <cstring>

using namespace NCL;

namespace ECS {

/**
 * @brief 从 Helldivers 2 指令码表中选取前缀无冲突的方向键序列。
 *
 * 使用 Knuth 乘法哈希从 45 条战略配置码中确定性地选取
 * replyCount 条不重复的指令码，赋给对应回复选项。
 * @param cs   聊天状态资源
 * @param seed 伪随机种子（通常为 dialoguePhase）
 */
static void GenerateDirSequences(Res_ChatState& cs, uint8_t seed) {
    bool used[kStratagemCount] = {};

    for (int i = 0; i < cs.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
        uint32_t h = static_cast<uint32_t>(seed) * 2654435761u
                   + static_cast<uint32_t>(i) * 40503u;
        uint32_t idx = (h >> 8) % kStratagemCount;

        while (used[idx]) {
            idx = (idx + 1) % kStratagemCount;
        }
        used[idx] = true;

        const auto& entry = kStratagems[idx];
        auto& seq = cs.replySequences[i];
        seq.length = entry.length;
        for (uint8_t k = 0; k < entry.length; ++k) {
            seq.keys[k] = entry.keys[k];
        }
    }

    cs.dirInputActive = true;
    ChatState_ClearDirInput(cs);
}

/**
 * @brief 根据当前 chatMode 返回对应的对话序列指针。
 * @param data     对话数据资源
 * @param chatMode 对话模式索引
 * @return 对应序列指针，越界时返回 nullptr
 */
static const DialogueSequence* GetSequence(const Res_DialogueData& data, uint8_t chatMode) {
    switch (chatMode) {
        case 0: return &data.proactive;
        case 1: return &data.mixed;
        case 2: return &data.passive;
        default: return nullptr;
    }
}

/**
 * @brief JSON 加载失败时填充硬编码对话数据（3 模式 × 1-3 节点）。
 * @param data 对话数据资源，函数结束后 data.loaded = true
 */
static void LoadFallbackDialogue(Res_DialogueData& data) {
    // Proactive — 3 nodes
    {
        auto& seq = data.proactive;
        seq.messageDelay = 8.0f;
        seq.nodeCount = 3;

        auto& n0 = seq.nodes[0];
        strncpy(n0.npcMessage, "All clear ahead. Ready to proceed?", sizeof(n0.npcMessage) - 1);
        strncpy(n0.replies[0], "Copy that, moving in.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Hold position.", sizeof(n0.replies[1]) - 1);
        strncpy(n0.replies[2], "What's the situation?", sizeof(n0.replies[2]) - 1);
        strncpy(n0.replies[3], "Let me scout first.", sizeof(n0.replies[3]) - 1);
        n0.effects[0] = 1; n0.effects[1] = 0; n0.effects[2] = 0; n0.effects[3] = 1;
        n0.replyCount = 4; n0.replyTimeLimit = 0.0f;

        auto& n1 = seq.nodes[1];
        strncpy(n1.npcMessage, "Intel suggests minimal resistance in the next sector.", sizeof(n1.npcMessage) - 1);
        strncpy(n1.replies[0], "Good, stay sharp.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[1], "Understood.", sizeof(n1.replies[1]) - 1);
        strncpy(n1.replies[2], "Any alternate routes?", sizeof(n1.replies[2]) - 1);
        strncpy(n1.replies[3], "I don't trust that intel.", sizeof(n1.replies[3]) - 1);
        n1.effects[0] = 1; n1.effects[1] = 0; n1.effects[2] = 0; n1.effects[3] = -1;
        n1.replyCount = 4; n1.replyTimeLimit = 0.0f;

        auto& n2 = seq.nodes[2];
        strncpy(n2.npcMessage, "We're making good progress. Extraction ready on your signal.", sizeof(n2.npcMessage) - 1);
        strncpy(n2.replies[0], "Acknowledged. Continuing.", sizeof(n2.replies[0]) - 1);
        strncpy(n2.replies[1], "Keep the line open.", sizeof(n2.replies[1]) - 1);
        strncpy(n2.replies[2], "How's our timeline?", sizeof(n2.replies[2]) - 1);
        strncpy(n2.replies[3], "Almost done here.", sizeof(n2.replies[3]) - 1);
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
        strncpy(n0.replies[1], "Push through.", sizeof(n0.replies[1]) - 1);
        strncpy(n0.replies[2], "Find cover.", sizeof(n0.replies[2]) - 1);
        strncpy(n0.replies[3], "Abort route.", sizeof(n0.replies[3]) - 1);
        n0.effects[0] = 1; n0.effects[1] = -1; n0.effects[2] = 1; n0.effects[3] = 0;
        n0.replyCount = 4; n0.replyTimeLimit = 12.0f;

        auto& n1 = seq.nodes[1];
        strncpy(n1.npcMessage, "Comms might be compromised. Keep it brief.", sizeof(n1.npcMessage) - 1);
        strncpy(n1.replies[0], "Roger. Eyes open.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[1], "Switching frequency.", sizeof(n1.replies[1]) - 1);
        strncpy(n1.replies[2], "How compromised?", sizeof(n1.replies[2]) - 1);
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
        strncpy(n0.replies[1], "Evading!", sizeof(n0.replies[1]) - 1);
        strncpy(n0.replies[2], "Need backup!", sizeof(n0.replies[2]) - 1);
        n0.effects[0] = -1; n0.effects[1] = 1; n0.effects[2] = 0;
        n0.replyCount = 3; n0.replyTimeLimit = 6.0f;
    }

    data.loaded = true;
}

/** @brief 初始化聊天状态和对话数据（JSON 优先，失败用 fallback）。 */
void Sys_Chat::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_ChatState>()) {
        registry.ctx_emplace<Res_ChatState>();
    }
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

/** @brief 每帧驱动对话流程：模式切换 / 方向键输入 / 回复确认 / 超时 / NPC 消息调度。 */
void Sys_Chat::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
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

    // ── Handle direction-key input for replies ─────────────
    const auto& input = registry.ctx<Res_Input>();
    int8_t confirmedReply = -1;

    if (chat.replyCount > 0 && chat.dirInputActive) {
        // Detect arrow key press → append to buffer
        DirKey pressed = DirKey::Up;
        bool hasPress = false;
        if      (input.keyPressed[KeyCodes::UP])    { pressed = DirKey::Up;    hasPress = true; }
        else if (input.keyPressed[KeyCodes::DOWN])  { pressed = DirKey::Down;  hasPress = true; }
        else if (input.keyPressed[KeyCodes::LEFT])  { pressed = DirKey::Left;  hasPress = true; }
        else if (input.keyPressed[KeyCodes::RIGHT]) { pressed = DirKey::Right; hasPress = true; }

        if (hasPress && chat.inputBufferLen < Res_ChatState::kInputBufferSize) {
            chat.inputBuffer[chat.inputBufferLen++] = pressed;

            // Check for exact match with any reply sequence
            bool anyPrefix = false;
            for (int i = 0; i < chat.replyCount; ++i) {
                const auto& seq = chat.replySequences[i];
                if (chat.inputBufferLen > seq.length) continue;

                // Check if current buffer matches this sequence's prefix
                bool match = true;
                for (uint8_t k = 0; k < chat.inputBufferLen; ++k) {
                    if (chat.inputBuffer[k] != seq.keys[k]) { match = false; break; }
                }

                if (match) {
                    if (chat.inputBufferLen == seq.length) {
                        confirmedReply = static_cast<int8_t>(i);
                        break;
                    }
                    anyPrefix = true;
                }
            }

            // If no sequence has this as a valid prefix → clear buffer
            if (confirmedReply < 0 && !anyPrefix) {
                ChatState_ClearDirInput(chat);
            }
        }
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
                GenerateDirSequences(chat, chat.dialoguePhase);

                // Direction-key input requires a countdown to drive urgency;
                // fall back to kDefaultReplyTime when JSON specifies 0 (no limit).
                constexpr float kDefaultReplyTime = 10.0f;
                float timeLimit = (node.replyTimeLimit > 0.0f) ? node.replyTimeLimit : kDefaultReplyTime;
                chat.replyTimerActive = true;
                chat.replyTimer       = timeLimit;
                chat.replyTimerMax    = timeLimit;

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
