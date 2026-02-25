#include "Sys_Chat.h"

#include "Window.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS {

// ============================================================
// 硬编码对话数据（Phase 4 最小可行版）
// ============================================================

struct DialogueNode {
    const char* npcMessage;
    struct Reply {
        const char* text;
        uint8_t     effect;   // 0=neutral, 1=good, 2=bad
    };
    Reply replies[4];
    uint8_t replyCount;
    float   replyTimeLimit;   // 0 = 无限时
};

// ── Proactive 模式对话树（Safe/Search）──
static const DialogueNode kProactiveDialogue[] = {
    {
        "ADMIN> System check initiated. Report your station ID.",
        {
            {"Station B-7, maintenance mode.",       1},
            {"Running diagnostics, standby.",        1},
            {"Who's asking?",                        2},
            {"...",                                  0},
        },
        4, 0.0f
    },
    {
        "ADMIN> Copy that. Any anomalies to report?",
        {
            {"Negative. All systems nominal.",       1},
            {"Minor fluctuations, investigating.",   0},
            {"Define 'anomalies'.",                  2},
        },
        3, 0.0f
    },
    {
        "ADMIN> Scheduled maintenance window closes in 30 min. Stay on task.",
        {
            {"Understood, proceeding as planned.",   1},
            {"Will need extra time. Authorize?",     0},
        },
        2, 0.0f
    },
};
static constexpr int kProactiveCount = 3;

// ── Mixed 模式对话树（Alert）──
static const DialogueNode kMixedDialogue[] = {
    {
        "ADMIN> We detected unusual traffic on your node. Explain.",
        {
            {"Routine data migration. Authorized.",  1},
            {"Running a backup script.",             0},
            {"Must be a false positive.",            2},
        },
        3, 12.0f
    },
    {
        "ADMIN> Security sweep in progress. Do NOT disconnect.",
        {
            {"Acknowledged. Standing by.",           1},
            {"How long will this take?",             0},
        },
        2, 10.0f
    },
};
static constexpr int kMixedCount = 2;

// ── Passive 模式对话树（Hunt/Raid）──
static const DialogueNode kPassiveDialogue[] = {
    {
        "ADMIN> INTRUDER ALERT. Authenticate NOW or face lockdown.",
        {
            {"Override code: ALPHA-7-NINER.",        1},
            {"System error, rebooting...",           2},
        },
        2, 6.0f
    },
};
static constexpr int kPassiveCount = 1;

// ============================================================
// 辅助函数
// ============================================================

static uint8_t CalcChatMode(float alertLevel) {
    AlertStatus status = GetAlertStatus(alertLevel);
    switch (status) {
        case AlertStatus::Safe:
        case AlertStatus::Search:
            return 0;  // proactive
        case AlertStatus::Alert:
            return 1;  // mixed
        case AlertStatus::Hunt:
        case AlertStatus::Raid:
            return 2;  // passive
        default:
            return 0;
    }
}

static const DialogueNode& GetDialogueNode(uint8_t mode, uint8_t phase) {
    switch (mode) {
        case 0:  return kProactiveDialogue[phase % kProactiveCount];
        case 1:  return kMixedDialogue[phase % kMixedCount];
        case 2:  return kPassiveDialogue[phase % kPassiveCount];
        default: return kProactiveDialogue[0];
    }
}

// ============================================================
// OnAwake
// ============================================================

void Sys_Chat::OnAwake(Registry& registry) {
    if (!registry.has_ctx<Res_ChatState>()) {
        registry.ctx_emplace<Res_ChatState>();
    }

    auto& chat = registry.ctx<Res_ChatState>();
    chat.PushMessage("SYSTEM> Secure channel established.", 0, 0.0f);
    chat.PushMessage("SYSTEM> Firewall admin online.", 0, 0.0f);

    LOG_INFO("[Sys_Chat] OnAwake — Chat state initialized.");
}

// ============================================================
// OnUpdate
// ============================================================

void Sys_Chat::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_ChatState>()) return;
    if (!registry.has_ctx<Res_GameplayState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;

    auto& chat = registry.ctx<Res_ChatState>();
    auto& gs   = registry.ctx<Res_GameplayState>();
    auto& ui   = registry.ctx<Res_UIState>();

    // 仅在HUD画面时运行对话逻辑
    if (ui.activeScreen != UIScreen::HUD) return;

    // ── 更新 chatMode ──
    chat.chatMode = CalcChatMode(gs.alertLevel);

    // ── 处理玩家回复输入 ──
    const Keyboard* kb = Window::GetKeyboard();
    if (chat.waitingForReply && chat.replyCount > 0 && kb) {
        int selectedIdx = -1;

        // 数字键 1-4 选择回复
        if (kb->KeyPressed(KeyCodes::NUM1) && chat.replyCount > 0) selectedIdx = 0;
        if (kb->KeyPressed(KeyCodes::NUM2) && chat.replyCount > 1) selectedIdx = 1;
        if (kb->KeyPressed(KeyCodes::NUM3) && chat.replyCount > 2) selectedIdx = 2;
        if (kb->KeyPressed(KeyCodes::NUM4) && chat.replyCount > 3) selectedIdx = 3;

        // Enter 确认当前选中的回复
        if (kb->KeyPressed(KeyCodes::RETURN) && chat.selectedReply >= 0
            && chat.selectedReply < chat.replyCount) {
            selectedIdx = chat.selectedReply;
        }

        // W/S 导航回复选项（仅在等待回复时）
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            chat.selectedReply = (chat.selectedReply - 1 + chat.replyCount) % chat.replyCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            chat.selectedReply = (chat.selectedReply + 1) % chat.replyCount;
        }

        if (selectedIdx >= 0 && selectedIdx < chat.replyCount) {
            // 先保存回复数据到局部变量（ClearReplies 会清零原数据）
            uint8_t effect = chat.replies[selectedIdx].effectType;

            // 追加玩家消息
            chat.PushMessage(chat.replies[selectedIdx].text, 1, ui.globalTime);

            // 效果：影响 alertLevel
            switch (effect) {
                case 1: // good — 减缓警戒
                    gs.alertLevel -= 5.0f;
                    if (gs.alertLevel < 0.0f) gs.alertLevel = 0.0f;
                    chat.PushMessage("SYSTEM> Response accepted. Trust +", 0, ui.globalTime);
                    break;
                case 2: // bad — 加速警戒
                    gs.alertLevel += 10.0f;
                    if (gs.alertLevel > gs.alertMax) gs.alertLevel = gs.alertMax;
                    chat.PushMessage("SYSTEM> Suspicion raised. Trust -", 0, ui.globalTime);
                    break;
                default: // neutral
                    chat.PushMessage("SYSTEM> Response noted.", 0, ui.globalTime);
                    break;
            }

            // 推进对话阶段
            chat.ClearReplies();
            chat.waitingForReply = false;
            chat.dialoguePhase++;
            chat.nextMessageTimer = chat.nextMessageDelay;

            LOG_INFO("[Sys_Chat] Reply selected: " << selectedIdx
                     << " effect=" << (int)effect);
        }
    }

    // ── 回复计时器 ──
    if (chat.replyTimerActive && chat.waitingForReply) {
        chat.replyTimer -= dt;
        if (chat.replyTimer <= 0.0f) {
            // 超时 — 视为最差结果
            chat.PushMessage("SYSTEM> NO RESPONSE - TIMEOUT", 0, ui.globalTime);
            gs.alertLevel += 15.0f;
            if (gs.alertLevel > gs.alertMax) gs.alertLevel = gs.alertMax;

            chat.ClearReplies();
            chat.waitingForReply = false;
            chat.dialoguePhase++;
            chat.nextMessageTimer = chat.nextMessageDelay * 0.5f; // 超时后NPC更快追问

            LOG_INFO("[Sys_Chat] Reply timeout! alertLevel -> " << gs.alertLevel);
        }
    }

    // ── NPC 消息生成 ──
    if (!chat.waitingForReply) {
        chat.nextMessageTimer -= dt;
        if (chat.nextMessageTimer <= 0.0f) {
            const auto& node = GetDialogueNode(chat.chatMode, chat.dialoguePhase);

            // NPC 发送消息
            chat.PushMessage(node.npcMessage, 2, ui.globalTime);

            // 设置回复选项
            chat.ClearReplies();
            for (uint8_t i = 0; i < node.replyCount; ++i) {
                chat.AddReply(node.replies[i].text, node.replies[i].effect);
            }

            // 设置回复计时器
            if (node.replyTimeLimit > 0.0f) {
                chat.replyTimer = node.replyTimeLimit;
                chat.replyTimerMax = node.replyTimeLimit;
                chat.replyTimerActive = true;
            } else {
                chat.replyTimerActive = false;
            }

            chat.waitingForReply = true;
            chat.selectedReply = 0;

            // 根据模式调整NPC消息间隔
            switch (chat.chatMode) {
                case 0: chat.nextMessageDelay = 8.0f;  break; // proactive: 从容
                case 1: chat.nextMessageDelay = 5.0f;  break; // mixed: 较快
                case 2: chat.nextMessageDelay = 3.0f;  break; // passive: 紧迫
                default: break;
            }
        }
    }
}

// ============================================================
// OnDestroy
// ============================================================

void Sys_Chat::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_Chat] OnDestroy.");
}

} // namespace ECS
