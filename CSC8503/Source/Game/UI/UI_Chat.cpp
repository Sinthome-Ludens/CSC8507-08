/**
 * @file UI_Chat.cpp
 * @brief 聊天面板渲染实现（消息列表 + 方向键回复 UI + 倒计时）。
 */
#include "UI_Chat.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

/**
 * @brief 渲染常驻右侧聊天面板（320px 全高），包含消息列表与回复区域。
 * @param registry ECS 注册表
 * @param dt       帧间隔（未使用）
 */
void RenderChatPanel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_ChatState>()) return;
    auto& chat = registry.ctx<Res_ChatState>();

    // 常驻面板：HUD 状态下始终渲染（chatOpen 由 Sys_UI 管理）
    if (!chat.chatOpen) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Panel dimensions — right side, full height
    float panelW = Res_ChatState::kPanelWidth;
    float panelX = displaySize.x - panelW;
    float panelY = 0.0f;
    float panelH = displaySize.y;

    // ── Background (opaque warm cream) ────────────────────
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(245, 238, 232, 255));

    // Left border line
    draw->AddLine(
        ImVec2(panelX, panelY),
        ImVec2(panelX, panelY + panelH),
        IM_COL32(200, 200, 200, 180), 2.0f);

    // ── Mode colors ───────────────────────────────────────
    ImU32 modeColor;
    const char* modeLabel;
    switch (chat.chatMode) {
        case 0:  // Proactive
            modeColor = IM_COL32(80, 200, 120, 220);
            modeLabel = "SECURE";
            break;
        case 1:  // Mixed
            modeColor = IM_COL32(220, 200, 0, 220);
            modeLabel = "ALERT";
            break;
        case 2:  // Passive
        default:
            modeColor = IM_COL32(220, 60, 40, 220);
            modeLabel = "CRITICAL";
            break;
    }

    // ── Header ────────────────────────────────────────────
    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    float headerH = 36.0f;
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + headerH),
        IM_COL32(16, 13, 10, 200));

    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(panelX + 12.0f, panelY + 8.0f),
        IM_COL32(245, 238, 232, 240), "COMMS TERMINAL");
    if (termFont) ImGui::PopFont();

    // Mode tag (right side of header)
    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 modeSize = ImGui::CalcTextSize(modeLabel);
    float tagX = panelX + panelW - modeSize.x - 12.0f;
    float tagY = panelY + 10.0f;
    // Tag background
    draw->AddRectFilled(
        ImVec2(tagX - 6.0f, tagY - 2.0f),
        ImVec2(tagX + modeSize.x + 6.0f, tagY + modeSize.y + 2.0f),
        modeColor, 2.0f);
    draw->AddText(ImVec2(tagX, tagY),
        IM_COL32(16, 13, 10, 255), modeLabel);
    if (smallFont) ImGui::PopFont();

    // ── Reply timer bar ───────────────────────────────────
    float contentY = panelY + headerH;
    if (chat.replyTimerActive && chat.replyTimerMax > 0.0f) {
        float ratio = std::clamp(chat.replyTimer / chat.replyTimerMax, 0.0f, 1.0f);
        float barH = 4.0f;
        float barW = panelW * ratio;

        ImU32 timerColor;
        if (ratio > 0.5f) {
            timerColor = IM_COL32(80, 200, 120, 220);      // green
        } else if (ratio > 0.25f) {
            timerColor = IM_COL32(220, 200, 0, 220);       // yellow
        } else {
            timerColor = IM_COL32(220, 60, 40, 220);       // red
        }

        draw->AddRectFilled(
            ImVec2(panelX, contentY),
            ImVec2(panelX + barW, contentY + barH),
            timerColor);
        contentY += barH + 2.0f;
    }

    // ── Messages area ─────────────────────────────────────
    ImFont* bodyFont = UITheme::GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    float msgStartY = contentY + 6.0f;

    // Reply area layout: padding(8) + countdown(36?) + inputBox(28) +
    // spacing(4) + replies(N*42) + hint(24).  No replies → hint only (24).
    constexpr float kReplyPad      =  8.0f;
    constexpr float kCountdownH    = 36.0f;
    constexpr float kInputBoxH     = 28.0f;
    constexpr float kReplySpacing  =  4.0f;
    constexpr float kReplyItemH    = 42.0f;
    constexpr float kHintLineH     = 24.0f;

    float replyAreaH = kHintLineH;
    if (chat.replyCount > 0) {
        replyAreaH  = kReplyPad;
        if (chat.replyTimerActive) replyAreaH += kCountdownH;
        replyAreaH += kInputBoxH + kReplySpacing;
        replyAreaH += chat.replyCount * kReplyItemH;
        replyAreaH += kHintLineH;
    }

    float msgEndY = panelY + panelH - replyAreaH;
    constexpr float kMsgLineH = 22.0f;

    // Calculate how many messages fit
    int maxVisible = (int)((msgEndY - msgStartY) / kMsgLineH);
    int startMsg = std::max(0, chat.messageCount - maxVisible);

    float msgY = msgStartY;
    for (int i = startMsg; i < chat.messageCount && i < Res_ChatState::kMaxMessages; ++i) {
        if (msgY > msgEndY) break;

        const auto& msg = chat.messages[i];

        // Sender color by type
        ImU32 senderColor;
        switch (msg.senderType) {
            case 1:  // Player
                senderColor = IM_COL32(252, 111, 41, 240);
                break;
            case 2:  // NPC
                senderColor = modeColor;
                break;
            default: // System
                senderColor = IM_COL32(160, 160, 160, 200);
                break;
        }

        // Sender tag
        char senderBuf[40];
        snprintf(senderBuf, sizeof(senderBuf), "[%s]", msg.sender);
        draw->AddText(ImVec2(panelX + 10.0f, msgY), senderColor, senderBuf);

        // Message text
        ImVec2 senderSize = ImGui::CalcTextSize(senderBuf);
        float textX = panelX + 10.0f + senderSize.x + 6.0f;

        // Simple word-wrap: just truncate if too long for now
        draw->AddText(ImVec2(textX, msgY),
            IM_COL32(16, 13, 10, 220), msg.text);
        msgY += kMsgLineH;
    }

    if (bodyFont) ImGui::PopFont();

    // ── Reply area separator ──────────────────────────────
    float replyTopY = panelY + panelH - replyAreaH;
    draw->AddLine(
        ImVec2(panelX + 8.0f, replyTopY),
        ImVec2(panelX + panelW - 8.0f, replyTopY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    if (chat.replyCount > 0) {
        float curY = replyTopY + 8.0f;
        ImFont* largeFont = UITheme::GetFont_TerminalLarge();

        // ── Countdown number (centered, 32px) ────────────
        if (chat.replyTimerActive && chat.replyTimerMax > 0.0f) {
            int secs = (int)std::ceil(chat.replyTimer);
            char timerBuf[8];
            snprintf(timerBuf, sizeof(timerBuf), "%d", secs);

            if (largeFont) ImGui::PushFont(largeFont);
            ImVec2 timerSize = ImGui::CalcTextSize(timerBuf);
            float timerX = panelX + (panelW - timerSize.x) * 0.5f;

            float ratio = std::clamp(chat.replyTimer / chat.replyTimerMax, 0.0f, 1.0f);
            ImU32 timerColor;
            if      (ratio > 0.5f)  timerColor = IM_COL32(80, 200, 120, 220);
            else if (ratio > 0.25f) timerColor = IM_COL32(220, 200, 0, 220);
            else                    timerColor = IM_COL32(220, 60, 40, 220);

            draw->AddText(ImVec2(timerX, curY), timerColor, timerBuf);
            if (largeFont) ImGui::PopFont();
            curY += kCountdownH;
        }

        // ── Helper: draw direction arrow triangle ────────
        // dir: 0=Up, 1=Down, 2=Left, 3=Right
        // cx,cy = center of the arrow cell, sz = half-size
        auto DrawArrow = [&](float cx, float cy, int dir, ImU32 color, float sz = 6.0f) {
            switch (dir) {
                case 0: // Up
                    draw->AddTriangleFilled(
                        ImVec2(cx, cy - sz), ImVec2(cx - sz, cy + sz), ImVec2(cx + sz, cy + sz), color);
                    break;
                case 1: // Down
                    draw->AddTriangleFilled(
                        ImVec2(cx, cy + sz), ImVec2(cx - sz, cy - sz), ImVec2(cx + sz, cy - sz), color);
                    break;
                case 2: // Left
                    draw->AddTriangleFilled(
                        ImVec2(cx - sz, cy), ImVec2(cx + sz, cy - sz), ImVec2(cx + sz, cy + sz), color);
                    break;
                case 3: // Right
                    draw->AddTriangleFilled(
                        ImVec2(cx + sz, cy), ImVec2(cx - sz, cy - sz), ImVec2(cx - sz, cy + sz), color);
                    break;
            }
        };

        // ── Input recognition box (8 slots, centered) ────
        constexpr float kSlotSize = 20.0f;
        constexpr float kSlotGap  = 4.0f;
        float totalSlotW = Res_ChatState::kInputBufferSize * kSlotSize + (Res_ChatState::kInputBufferSize - 1) * kSlotGap;
        float slotStartX = panelX + (panelW - totalSlotW) * 0.5f;

        for (int s = 0; s < Res_ChatState::kInputBufferSize; ++s) {
            float sx = slotStartX + s * (kSlotSize + kSlotGap);
            float sy = curY;

            if (s < chat.inputBufferLen) {
                // Filled slot — orange border + arrow
                draw->AddRect(ImVec2(sx, sy), ImVec2(sx + kSlotSize, sy + kSlotSize),
                    IM_COL32(252, 111, 41, 255), 2.0f, 0, 2.0f);
                DrawArrow(sx + kSlotSize * 0.5f, sy + kSlotSize * 0.5f,
                    static_cast<int>(chat.inputBuffer[s]), IM_COL32(252, 111, 41, 255), 5.0f);
            } else {
                // Empty slot — gray dashed border
                draw->AddRect(ImVec2(sx, sy), ImVec2(sx + kSlotSize, sy + kSlotSize),
                    IM_COL32(160, 160, 160, 100), 2.0f, 0, 1.0f);
            }
        }
        curY += kInputBoxH;

        // ── Determine which reply is prefix-matched ──────
        int matchedReply = -1;
        if (chat.inputBufferLen > 0) {
            for (int i = 0; i < chat.replyCount; ++i) {
                const auto& seq = chat.replySequences[i];
                if (chat.inputBufferLen > seq.length) continue;
                bool match = true;
                for (uint8_t k = 0; k < chat.inputBufferLen; ++k) {
                    if (chat.inputBuffer[k] != seq.keys[k]) { match = false; break; }
                }
                if (match) { matchedReply = i; break; }
            }
        }

        // ── Reply options (text + direction sequence) ─────
        if (bodyFont) ImGui::PushFont(bodyFont);
        curY += kReplySpacing;
        for (int i = 0; i < chat.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
            bool isMatch = (i == matchedReply);

            // Highlight background for matched option
            if (isMatch) {
                draw->AddRectFilled(
                    ImVec2(panelX + 6.0f, curY - 2.0f),
                    ImVec2(panelX + panelW - 6.0f, curY + 40.0f),
                    IM_COL32(252, 111, 41, 30), 2.0f);
            }

            // Text line
            ImU32 textColor = isMatch ? IM_COL32(252, 111, 41, 255)
                                      : IM_COL32(16, 13, 10, 160);
            draw->AddText(ImVec2(panelX + 10.0f, curY), textColor, chat.replies[i].text);

            // Direction sequence arrows (below text)
            const auto& seq = chat.replySequences[i];
            float arrowY = curY + 22.0f;
            float arrowX = panelX + 14.0f;
            constexpr float kArrowCellW = 18.0f;

            for (uint8_t k = 0; k < seq.length; ++k) {
                // Bright if matched so far, dim otherwise
                bool keyMatched = isMatch && (k < chat.inputBufferLen);
                ImU32 arrowColor = keyMatched ? IM_COL32(252, 111, 41, 255)
                                             : IM_COL32(16, 13, 10, 80);
                DrawArrow(arrowX + kArrowCellW * 0.5f, arrowY + 7.0f,
                    static_cast<int>(seq.keys[k]), arrowColor, 5.0f);
                arrowX += kArrowCellW;
            }

            curY += kReplyItemH;
        }
        if (bodyFont) ImGui::PopFont();

        // ── Bottom hint ──────────────────────────────────
        if (smallFont) ImGui::PushFont(smallFont);
        const char* hintText = "[ARROW KEYS] INPUT";
        ImVec2 hintSize = ImGui::CalcTextSize(hintText);
        float hintX = panelX + (panelW - hintSize.x) * 0.5f;
        draw->AddText(ImVec2(hintX, panelY + panelH - 18.0f),
            IM_COL32(16, 13, 10, 100), hintText);
        if (smallFont) ImGui::PopFont();
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
