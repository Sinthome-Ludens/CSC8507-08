#pragma once

#include <cstdint>
#include <cstdio>

namespace ECS {

/**
 * @brief 聊天对话系统全局状态资源
 *
 * 存储防火墙管理员聊天面板的所有运行时数据。
 * 由 Sys_Chat 写入对话逻辑，由 UI_Chat 读取渲染。
 *
 * 设计：
 *   - 消息使用环形缓冲（固定16条，新消息覆盖最旧）
 *   - 回复选项最多4个，由对话树节点决定
 *   - chatMode 由 AlertStatus 驱动：Safe/Search=proactive, Alert=mixed, Hunt/Raid=passive
 *
 * @note 作为 Registry Context 资源（非 per-entity Component），
 *       不受 64 字节限制，但仍保持 POD 特性（无动态分配）。
 */
struct Res_ChatState {
    /// 聊天面板固定宽度（左右分屏布局用）
    static constexpr float PANEL_WIDTH = 320.0f;

    // ── 消息历史（环形缓冲）──
    struct ChatMessage {
        char    text[64] = {};
        uint8_t sender   = 0;     // 0=system, 1=player, 2=admin(NPC)
        float   timestamp = 0.0f;
        bool    used     = false;  // 此槽位是否被占用
    };
    static constexpr int MAX_MESSAGES = 16;
    ChatMessage messages[MAX_MESSAGES] = {};
    uint8_t messageHead  = 0;   // 下一个写入位置
    uint8_t messageCount = 0;   // 当前有效消息数

    // ── 回复选项 ──
    struct ReplyOption {
        char    text[48] = {};
        uint8_t effectType = 0;   // 0=neutral, 1=good(减缓警戒), 2=bad(加速警戒)
        bool    available  = false;
    };
    static constexpr int MAX_REPLIES = 4;
    ReplyOption replies[MAX_REPLIES] = {};
    uint8_t replyCount       = 0;
    int8_t  selectedReply    = 0;

    // ── 回复计时器 ──
    float   replyTimer       = 0.0f;
    float   replyTimerMax    = 10.0f;
    bool    replyTimerActive = false;

    // ── 对话模式 ──
    // 0=proactive (Safe/Search): NPC主动聊天，玩家从容回复
    // 1=mixed (Alert): NPC质疑，限时回复
    // 2=passive (Hunt/Raid): 仅紧急快速反应
    uint8_t chatMode = 0;

    // ── 对话树进度 ──
    uint8_t dialoguePhase = 0;   // 当前对话阶段 [0..N]
    float   nextMessageTimer = 0.0f;  // NPC下一条消息的倒计时
    float   nextMessageDelay = 5.0f;  // NPC消息间隔（秒）
    bool    waitingForReply  = false;  // 当前是否等待玩家回复

    // ── 辅助方法（内联，POD友好）──

    /// 追加一条消息到环形缓冲
    void PushMessage(const char* text, uint8_t sender, float time) {
        auto& msg = messages[messageHead];
        snprintf(msg.text, sizeof(msg.text), "%s", text);
        msg.sender    = sender;
        msg.timestamp = time;
        msg.used      = true;
        messageHead = (messageHead + 1) % MAX_MESSAGES;
        if (messageCount < MAX_MESSAGES) ++messageCount;
    }

    /// 获取第i条消息（0=最旧，messageCount-1=最新）
    const ChatMessage& GetMessage(int i) const {
        int idx = (messageHead - messageCount + i + MAX_MESSAGES) % MAX_MESSAGES;
        return messages[idx];
    }

    /// 清空所有回复选项
    void ClearReplies() {
        for (auto& r : replies) {
            r.text[0] = '\0';
            r.available = false;
            r.effectType = 0;
        }
        replyCount = 0;
        selectedReply = 0;
        replyTimerActive = false;
    }

    /// 添加一个回复选项
    void AddReply(const char* text, uint8_t effect) {
        if (replyCount >= MAX_REPLIES) return;
        auto& r = replies[replyCount];
        snprintf(r.text, sizeof(r.text), "%s", text);
        r.effectType = effect;
        r.available = true;
        ++replyCount;
    }
};

} // namespace ECS
