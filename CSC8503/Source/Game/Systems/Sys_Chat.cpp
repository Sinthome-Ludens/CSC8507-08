/**
 * @file Sys_Chat.cpp
 * @brief 聊天/对话系统实现（网状叙事图版本）。
 *
 * - OnAwake: 从 3 个 JSON 文件加载对话数据并初始化 Res_ChatState / Res_DialogueData。
 * - OnUpdate: 驱动网状对话流程（节点跳转 / 方向键输入 / 倒计时 / 回复确认）。
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
#include "Game/Components/Res_InputConfig.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/StratagemTable.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/Utils/DialogueLoader.h"
#include "Game/Utils/Log.h"
#include "Game/Events/Evt_Audio.h"
#include "Core/ECS/EventBus.h"

#include <algorithm>
#include <cstring>
#include <random>

using namespace NCL;

namespace ECS {

/**
 * @brief 从 Helldivers 2 指令码表中选取前缀无冲突的方向键序列。
 */
static void GenerateDirSequences(Res_ChatState& cs, uint32_t seed) {
    bool used[kStratagemCount] = {};

    for (int i = 0; i < cs.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
        uint32_t h = seed * 2654435761u
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
 * @brief 在序列中按 id 查找节点，返回指针（未找到返回 nullptr）。
 */
static const DialogueNode* FindNodeByID(const DialogueSequence& seq, const char* id) {
    for (int i = 0; i < seq.nodeCount; ++i) {
        if (std::strcmp(seq.nodes[i].id, id) == 0) {
            return &seq.nodes[i];
        }
    }
    return nullptr;
}

/**
 * @brief 简单字符串哈希，用于 GenerateDirSequences 的 seed。
 */
static uint32_t HashNodeId(const char* id) {
    uint32_t h = 5381;
    while (*id) {
        h = ((h << 5) + h) + static_cast<uint8_t>(*id++);
    }
    return h;
}

/**
 * @brief 从对话序列的多棵树中随机选一棵，将其 rootNodeId 写入 dst。
 */
static bool PickRandomTree(const DialogueSequence& seq, char* dst, int dstSize) {
    if (seq.treeCount <= 0) return false;
    static std::mt19937 rng(std::random_device{}());
    int idx = std::uniform_int_distribution<int>(0, seq.treeCount - 1)(rng);
    strncpy(dst, seq.trees[idx].rootNodeId, dstSize - 1);
    dst[dstSize - 1] = '\0';
    return true;
}

/**
 * @brief 按 treeId 查找指定对话树，将其 rootNodeId 写入 dst。
 */
static bool FindTreeById(const DialogueSequence& seq, const char* treeId, char* dst, int dstSize) {
    for (int i = 0; i < seq.treeCount; ++i) {
        if (std::strcmp(seq.trees[i].treeId, treeId) == 0) {
            strncpy(dst, seq.trees[i].rootNodeId, dstSize - 1);
            dst[dstSize - 1] = '\0';
            return true;
        }
    }
    return false;
}

/**
 * @brief 选择对话树：forcedTreeId 非空则指定，否则随机。
 */
static bool SelectTree(const DialogueSequence& seq, const char* forcedTreeId,
                       char* dst, int dstSize) {
    if (forcedTreeId[0] != '\0') {
        if (FindTreeById(seq, forcedTreeId, dst, dstSize)) return true;
        LOG_WARN("[Sys_Chat] Forced treeId '" << forcedTreeId << "' not found, falling back to random");
    }
    return PickRandomTree(seq, dst, dstSize);
}

/**
 * @brief JSON 加载失败时填充硬编码对话数据（网状图版本）。
 */
static void LoadFallbackDialogue(Res_DialogueData& data) {
    // Proactive — 3 nodes
    {
        auto& seq = data.proactive;
        seq.messageDelay = 8.0f;
        seq.nodeCount = 3;
        seq.treeCount = 1;
        strncpy(seq.trees[0].treeId, "fallback_p", sizeof(seq.trees[0].treeId) - 1);
        strncpy(seq.trees[0].rootNodeId, "p_01", sizeof(seq.trees[0].rootNodeId) - 1);

        auto& n0 = seq.nodes[0];
        strncpy(n0.id, "p_01", sizeof(n0.id) - 1);
        strncpy(n0.speaker, "HANDLER", sizeof(n0.speaker) - 1);
        strncpy(n0.npcMessage, "All clear ahead. Ready to proceed?", sizeof(n0.npcMessage) - 1);
        n0.alertDelta = 0.0f;
        strncpy(n0.replies[0], "Copy that, moving in.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Hold position.", sizeof(n0.replies[1]) - 1);
        strncpy(n0.replies[2], "What's the situation?", sizeof(n0.replies[2]) - 1);
        strncpy(n0.replies[3], "Let me scout first.", sizeof(n0.replies[3]) - 1);
        n0.replyAlertDelta[0] = -5; n0.replyAlertDelta[1] = 0; n0.replyAlertDelta[2] = 0; n0.replyAlertDelta[3] = -5;
        strncpy(n0.nextNodeId[0], "p_02", sizeof(n0.nextNodeId[0]) - 1);
        strncpy(n0.nextNodeId[1], "p_02", sizeof(n0.nextNodeId[1]) - 1);
        strncpy(n0.nextNodeId[2], "p_03", sizeof(n0.nextNodeId[2]) - 1);
        strncpy(n0.nextNodeId[3], "p_03", sizeof(n0.nextNodeId[3]) - 1);
        n0.replyCount = 4; n0.replyTimeLimit = 0.0f;

        auto& n1 = seq.nodes[1];
        strncpy(n1.id, "p_02", sizeof(n1.id) - 1);
        strncpy(n1.speaker, "HANDLER", sizeof(n1.speaker) - 1);
        strncpy(n1.npcMessage, "Intel suggests minimal resistance in the next sector.", sizeof(n1.npcMessage) - 1);
        n1.alertDelta = 0.0f;
        strncpy(n1.replies[0], "Good, stay sharp.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[1], "Understood.", sizeof(n1.replies[1]) - 1);
        strncpy(n1.replies[2], "Any alternate routes?", sizeof(n1.replies[2]) - 1);
        strncpy(n1.replies[3], "I don't trust that intel.", sizeof(n1.replies[3]) - 1);
        n1.replyAlertDelta[0] = -5; n1.replyAlertDelta[1] = 0; n1.replyAlertDelta[2] = 0; n1.replyAlertDelta[3] = 10;
        strncpy(n1.nextNodeId[0], "p_03", sizeof(n1.nextNodeId[0]) - 1);
        strncpy(n1.nextNodeId[1], "p_03", sizeof(n1.nextNodeId[1]) - 1);
        n1.replyCount = 4; n1.replyTimeLimit = 0.0f;

        auto& n2 = seq.nodes[2];
        strncpy(n2.id, "p_03", sizeof(n2.id) - 1);
        strncpy(n2.speaker, "HANDLER", sizeof(n2.speaker) - 1);
        strncpy(n2.npcMessage, "We're making good progress. Extraction ready on your signal.", sizeof(n2.npcMessage) - 1);
        n2.alertDelta = -3.0f;
        strncpy(n2.replies[0], "Acknowledged. Continuing.", sizeof(n2.replies[0]) - 1);
        strncpy(n2.replies[1], "Keep the line open.", sizeof(n2.replies[1]) - 1);
        strncpy(n2.replies[2], "How's our timeline?", sizeof(n2.replies[2]) - 1);
        strncpy(n2.replies[3], "Almost done here.", sizeof(n2.replies[3]) - 1);
        n2.replyAlertDelta[0] = -5; n2.replyAlertDelta[1] = 0; n2.replyAlertDelta[2] = 0; n2.replyAlertDelta[3] = -5;
        n2.replyCount = 4; n2.replyTimeLimit = 0.0f;
    }

    // Mixed — 2 nodes
    {
        auto& seq = data.mixed;
        seq.messageDelay = 5.0f;
        seq.nodeCount = 2;
        seq.treeCount = 1;
        strncpy(seq.trees[0].treeId, "fallback_m", sizeof(seq.trees[0].treeId) - 1);
        strncpy(seq.trees[0].rootNodeId, "m_01", sizeof(seq.trees[0].rootNodeId) - 1);

        auto& n0 = seq.nodes[0];
        strncpy(n0.id, "m_01", sizeof(n0.id) - 1);
        strncpy(n0.speaker, "HANDLER", sizeof(n0.speaker) - 1);
        strncpy(n0.npcMessage, "Movement detected nearby. What's your call?", sizeof(n0.npcMessage) - 1);
        n0.alertDelta = 2.0f;
        strncpy(n0.replies[0], "Go silent.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Push through.", sizeof(n0.replies[1]) - 1);
        strncpy(n0.replies[2], "Find cover.", sizeof(n0.replies[2]) - 1);
        strncpy(n0.replies[3], "Abort route.", sizeof(n0.replies[3]) - 1);
        n0.replyAlertDelta[0] = -5; n0.replyAlertDelta[1] = 10; n0.replyAlertDelta[2] = -5; n0.replyAlertDelta[3] = 0;
        strncpy(n0.nextNodeId[0], "m_02", sizeof(n0.nextNodeId[0]) - 1);
        strncpy(n0.nextNodeId[1], "m_02", sizeof(n0.nextNodeId[1]) - 1);
        strncpy(n0.nextNodeId[2], "m_02", sizeof(n0.nextNodeId[2]) - 1);
        n0.replyCount = 4; n0.replyTimeLimit = 12.0f;

        auto& n1 = seq.nodes[1];
        strncpy(n1.id, "m_02", sizeof(n1.id) - 1);
        strncpy(n1.speaker, "HANDLER", sizeof(n1.speaker) - 1);
        strncpy(n1.npcMessage, "Comms might be compromised. Keep it brief.", sizeof(n1.npcMessage) - 1);
        n1.alertDelta = 3.0f;
        strncpy(n1.replies[0], "Roger. Eyes open.", sizeof(n1.replies[0]) - 1);
        strncpy(n1.replies[1], "Switching frequency.", sizeof(n1.replies[1]) - 1);
        strncpy(n1.replies[2], "How compromised?", sizeof(n1.replies[2]) - 1);
        n1.replyAlertDelta[0] = -5; n1.replyAlertDelta[1] = 0; n1.replyAlertDelta[2] = 8;
        n1.replyCount = 3; n1.replyTimeLimit = 10.0f;
    }

    // Passive — 1 node
    {
        auto& seq = data.passive;
        seq.messageDelay = 3.0f;
        seq.nodeCount = 1;
        seq.treeCount = 1;
        strncpy(seq.trees[0].treeId, "fallback_x", sizeof(seq.trees[0].treeId) - 1);
        strncpy(seq.trees[0].rootNodeId, "x_01", sizeof(seq.trees[0].rootNodeId) - 1);

        auto& n0 = seq.nodes[0];
        strncpy(n0.id, "x_01", sizeof(n0.id) - 1);
        strncpy(n0.speaker, "HANDLER", sizeof(n0.speaker) - 1);
        strncpy(n0.npcMessage, "You've been spotted! Respond NOW!", sizeof(n0.npcMessage) - 1);
        n0.alertDelta = 5.0f;
        strncpy(n0.replies[0], "Engaging.", sizeof(n0.replies[0]) - 1);
        strncpy(n0.replies[1], "Evading!", sizeof(n0.replies[1]) - 1);
        strncpy(n0.replies[2], "Need backup!", sizeof(n0.replies[2]) - 1);
        n0.replyAlertDelta[0] = 10; n0.replyAlertDelta[1] = -5; n0.replyAlertDelta[2] = 0;
        n0.replyCount = 3; n0.replyTimeLimit = 6.0f;
    }

    data.loaded = true;
}

/** @brief 初始化聊天状态和对话数据（3 个 JSON 文件，失败用 fallback）。 */
void Sys_Chat::OnAwake(Registry& registry) {
    bool chatStateExisted = registry.has_ctx<Res_ChatState>();

    if (!chatStateExisted) {
        registry.ctx_emplace<Res_ChatState>();
    }
    auto& chat = registry.ctx<Res_ChatState>();

    // Load dialogue data from 3 separate JSON files
    if (!registry.has_ctx<Res_DialogueData>()) {
        registry.ctx_emplace<Res_DialogueData>();
    }
    auto& dialogueData = registry.ctx<Res_DialogueData>();

    if (!dialogueData.loaded) {
        const std::string dir = NCL::Assets::ASSETROOT + "Dialogue/";
        bool ok = true;
        ok &= LoadDialogueSequenceFromJSON(dir + "Dialogue_Normal.json",  dialogueData.proactive);
        ok &= LoadDialogueSequenceFromJSON(dir + "Dialogue_Alert.json",   dialogueData.mixed);
        ok &= LoadDialogueSequenceFromJSON(dir + "Dialogue_Exposed.json", dialogueData.passive);

        if (ok) {
            dialogueData.loaded = true;
            LOG_INFO("[Sys_Chat] Loaded 3 dialogue files: proactive="
                     << dialogueData.proactive.nodeCount << " mixed="
                     << dialogueData.mixed.nodeCount << " passive="
                     << dialogueData.passive.nodeCount);
        } else {
            LOG_WARN("[Sys_Chat] JSON load failed, using fallback dialogue");
            LoadFallbackDialogue(dialogueData);
        }
    }

    if (!chatStateExisted) {
        ChatState_PushMessage(chat, "SYSTEM", "COMMS LINK ESTABLISHED", 0, true);
        ChatState_PushMessage(chat, "SYSTEM", "Awaiting operator input...", 0, true);
    }

    // Always reset dialogue progress — select a dialogue tree (forced or random)
    // Note: forcedTreeId may have been set by the scene *before* OnAwake (e.g., TutorialLevel).
    // We read it first, then clear it so it doesn't persist to the next map.
    const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
    if (!seq || !SelectTree(*seq, chat.forcedTreeId, chat.currentNodeId, sizeof(chat.currentNodeId))) {
        chat.currentNodeId[0] = '\0';
    }
    // Clear forcedTreeId after tree selection so it doesn't carry over to the next map
    chat.forcedTreeId[0]  = '\0';
    chat.treeFinished     = false;
    chat.nextMessageDelay = seq ? seq->messageDelay : 8.0f;
    chat.nextMessageTimer = 3.0f;
    ChatState_ClearReplies(chat);

    LOG_INFO("[Sys_Chat] OnAwake — Chat initialized, dialogue data "
             << (dialogueData.loaded ? "loaded" : "MISSING")
             << ", crossMap=" << (chatStateExisted ? "yes" : "no"));
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
    if (alertLevel <= 50.0f) {
        newMode = 0;
    } else if (alertLevel <= 75.0f) {
        newMode = 1;
    } else {
        newMode = 2;
    }

    if (newMode != chat.chatMode) {
        chat.chatMode = newMode;
        ChatState_ClearReplies(chat);

        // If the tree already finished, do NOT start a new one on mode switch
        if (!chat.treeFinished) {
            const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
            if (!seq || !SelectTree(*seq, chat.forcedTreeId, chat.currentNodeId, sizeof(chat.currentNodeId))) {
                chat.currentNodeId[0] = '\0';
                chat.treeFinished = true;
            }
            chat.nextMessageDelay = seq ? seq->messageDelay : 8.0f;
            chat.nextMessageTimer = 1.0f;
        }
        LOG_INFO("[Sys_Chat] Mode changed to " << (int)chat.chatMode
                 << (chat.treeFinished ? " (tree already finished)" : ""));
    }

    // ── Handle direction-key input for replies ─────────────
    const auto& input = registry.ctx<Res_Input>();
    int8_t confirmedReply = -1;

    Res_InputConfig defaultCfg;
    const auto& cfg = registry.has_ctx<Res_InputConfig>() ? registry.ctx<Res_InputConfig>() : defaultCfg;

    if (chat.replyCount > 0 && chat.dirInputActive) {
        DirKey pressed = DirKey::Up;
        bool hasPress = false;
        if      (input.keyPressed[cfg.keyChatUp])    { pressed = DirKey::Up;    hasPress = true; }
        else if (input.keyPressed[cfg.keyChatDown])  { pressed = DirKey::Down;  hasPress = true; }
        else if (input.keyPressed[cfg.keyChatLeft])  { pressed = DirKey::Left;  hasPress = true; }
        else if (input.keyPressed[cfg.keyChatRight]) { pressed = DirKey::Right; hasPress = true; }

        if (hasPress) {
            if (registry.has_ctx<EventBus*>()) {
                auto* bus = registry.ctx<EventBus*>();
                if (bus) bus->publish_deferred<Evt_Audio_PlaySFX>(Evt_Audio_PlaySFX{SfxId::UIClick});
            }
        }
        if (hasPress && chat.inputBufferLen < Res_ChatState::kInputBufferSize) {
            chat.inputBuffer[chat.inputBufferLen++] = pressed;

            bool anyPrefix = false;
            for (int i = 0; i < chat.replyCount; ++i) {
                const auto& seq = chat.replySequences[i];
                if (chat.inputBufferLen > seq.length) continue;

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

            if (confirmedReply < 0 && !anyPrefix) {
                ChatState_ClearDirInput(chat);
            }
        }
    }

    // ── Process confirmed reply (graph traversal) ─────────
    if (confirmedReply >= 0 && confirmedReply < chat.replyCount) {
        auto& reply = chat.replies[confirmedReply];

        ChatState_PushMessage(chat, "YOU", reply.text, 1, false);

        // Apply reply alertDelta
        if (registry.has_ctx<Res_GameState>() && reply.alertDelta != 0.0f) {
            auto& gs = registry.ctx<Res_GameState>();
            gs.alertLevel = std::clamp(gs.alertLevel + reply.alertDelta, 0.0f, gs.alertMax);
            LOG_INFO("[Sys_Chat] Reply alertDelta " << reply.alertDelta
                     << " → alertLevel=" << gs.alertLevel);
        }

        // Graph jump: read nextNodeId from current node
        const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
        if (seq) {
            const DialogueNode* curNode = FindNodeByID(*seq, chat.currentNodeId);
            if (curNode && curNode->nextNodeId[confirmedReply][0] != '\0') {
                // Jump to specified next node
                strncpy(chat.currentNodeId, curNode->nextNodeId[confirmedReply],
                        sizeof(chat.currentNodeId) - 1);
                chat.currentNodeId[sizeof(chat.currentNodeId) - 1] = '\0';
            } else if (curNode && curNode->isLoop) {
                // isLoop: stay on the same node — will re-display after delay
                // (currentNodeId unchanged)
            } else {
                // End of branch — mark tree as finished immediately
                chat.currentNodeId[0] = '\0';
                chat.treeFinished = true;
            }
        }

        ChatState_ClearReplies(chat);
        chat.nextMessageTimer = 2.0f;
        chat.waitingForReply  = false;
    }

    // ── Reply timer (timeout) ─────────────────────────────
    if (chat.replyTimerActive && chat.replyCount > 0) {
        chat.replyTimer -= dt;
        if (chat.replyTimer <= 0.0f) {
            ChatState_PushMessage(chat, "SYSTEM", "Response timeout — alert increased", 0, true);
            if (registry.has_ctx<EventBus*>()) {
                auto* bus = registry.ctx<EventBus*>();
                if (bus) bus->publish_deferred<Evt_Audio_PlaySFX>(Evt_Audio_PlaySFX{SfxId::DialogueTimeout});
            }
            if (registry.has_ctx<Res_GameState>()) {
                auto& gs = registry.ctx<Res_GameState>();
                gs.alertLevel = std::min(gs.alertMax, gs.alertLevel + 15.0f);
            }

            // isLoop nodes: stay on the same node instead of ending the tree
            const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
            const DialogueNode* curNode = seq ? FindNodeByID(*seq, chat.currentNodeId) : nullptr;
            if (curNode && curNode->isLoop) {
                // keep currentNodeId unchanged — will re-display after delay
            } else {
                // Timeout ended the tree — mark finished immediately
                chat.currentNodeId[0] = '\0';
                chat.treeFinished = true;
            }

            ChatState_ClearReplies(chat);
            chat.nextMessageTimer = 2.0f;
            LOG_INFO("[Sys_Chat] Reply timeout — alert +15"
                     << (curNode && curNode->isLoop ? " (isLoop: re-showing)" : ""));
        }
    }

    // ── NPC message scheduling (graph-driven) ─────────────
    if (!chat.waitingForReply && chat.replyCount == 0) {
        // Tree finished — stop scheduling new messages
        if (chat.treeFinished) return;

        chat.nextMessageTimer -= dt;
        if (chat.nextMessageTimer <= 0.0f) {
            const DialogueSequence* seq = GetSequence(dialogueData, chat.chatMode);
            if (!seq) return;

            // If currentNodeId is empty, tree has ended
            if (chat.currentNodeId[0] == '\0') {
                chat.treeFinished = true;
                return;
            }

            const DialogueNode* node = FindNodeByID(*seq, chat.currentNodeId);
            if (node) {
                // Use speaker from JSON; "YOU" → senderType 1 (player), others → 2 (NPC)
                const char* speaker = (node->speaker[0] != '\0') ? node->speaker : "HANDLER";
                bool isPlayerLine = (std::strcmp(speaker, "YOU") == 0);
                ChatState_PushMessage(chat, speaker, node->npcMessage,
                                      isPlayerLine ? 1 : 2, false);
                if (registry.has_ctx<EventBus*>()) {
                    auto* bus = registry.ctx<EventBus*>();
                    if (bus) bus->publish_deferred<Evt_Audio_PlaySFX>(Evt_Audio_PlaySFX{SfxId::DialoguePopup});
                }

                // Apply node-level alertDelta when displayed
                if (registry.has_ctx<Res_GameState>() && node->alertDelta != 0.0f) {
                    auto& gs = registry.ctx<Res_GameState>();
                    gs.alertLevel = std::clamp(gs.alertLevel + node->alertDelta, 0.0f, gs.alertMax);
                    LOG_INFO("[Sys_Chat] Node '" << node->id << "' alertDelta "
                             << node->alertDelta << " → alertLevel=" << gs.alertLevel);
                }

                if (node->waitReply) {
                    // ── Interactive node: show replies, wait for player input ──
                    for (int i = 0; i < node->replyCount; ++i) {
                        ChatState_AddReply(chat, node->replies[i], node->replyAlertDelta[i]);
                    }
                    chat.selectedReply = 0;
                    GenerateDirSequences(chat, HashNodeId(chat.currentNodeId));

                    constexpr float kDefaultReplyTime = 10.0f;
                    float timeLimit = (node->replyTimeLimit > 0.0f) ? node->replyTimeLimit : kDefaultReplyTime;
                    chat.replyTimerActive = true;
                    chat.replyTimer       = timeLimit;
                    chat.replyTimerMax    = timeLimit;

                    chat.waitingForReply = true;
                } else {
                    // ── Auto-advance node: no player input, jump after delay ──
                    if (node->autoNextId[0] != '\0') {
                        strncpy(chat.currentNodeId, node->autoNextId,
                                sizeof(chat.currentNodeId) - 1);
                        chat.currentNodeId[sizeof(chat.currentNodeId) - 1] = '\0';
                    } else {
                        chat.currentNodeId[0] = '\0';
                    }
                    chat.nextMessageTimer = seq->messageDelay;
                }
            } else {
                LOG_WARN("[Sys_Chat] Node '" << chat.currentNodeId << "' not found, restarting");
                chat.currentNodeId[0] = '\0';
                chat.nextMessageTimer = seq->messageDelay;
            }
        }
    }
}

} // namespace ECS

#ifdef _MSC_VER
#pragma warning(pop)
#endif
