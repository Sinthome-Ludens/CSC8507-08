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

using namespace ECS::UITheme;

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
        Col32_Bg());

    // Left border line
    draw->AddLine(
        ImVec2(panelX, panelY),
        ImVec2(panelX, panelY + panelH),
        Col32_Gray(180), 2.0f);

    // ── Mode colors ───────────────────────────────────────
    ImU32 modeColor;
    const char* modeLabel;
    switch (chat.chatMode) {
        case 0:  // Proactive
            modeColor = Col32_Green();
            modeLabel = "SECURE";
            break;
        case 1:  // Mixed
            modeColor = Col32_Yellow();
            modeLabel = "ALERT";
            break;
        case 2:  // Passive
        default:
            modeColor = Col32_Red();
            modeLabel = "CRITICAL";
            break;
    }

    // ── Header ────────────────────────────────────────────
    ImFont* termFont  = GetFont_Terminal();
    ImFont* smallFont = GetFont_Small();

    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + Layout::Chat::kHeaderH),
        Col32_BgDark(200));

    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(panelX + Layout::Chat::kHeaderPadX, panelY + Layout::Chat::kHeaderTextY),
        Col32_Bg(240), "COMMS TERMINAL");
    if (termFont) ImGui::PopFont();

    // Mode tag (right side of header)
    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 modeSize = ImGui::CalcTextSize(modeLabel);
    float tagX = panelX + panelW - modeSize.x - Layout::Chat::kHeaderPadX;
    float tagY = panelY + Layout::Chat::kModeTagY;
    // Tag background
    draw->AddRectFilled(
        ImVec2(tagX - Layout::Chat::kModeTagPadX, tagY - Layout::Chat::kModeTagPadY),
        ImVec2(tagX + modeSize.x + Layout::Chat::kModeTagPadX, tagY + modeSize.y + Layout::Chat::kModeTagPadY),
        modeColor, 2.0f);
    draw->AddText(ImVec2(tagX, tagY),
        Col32_Text(), modeLabel);
    if (smallFont) ImGui::PopFont();

    // ── Reply timer bar ───────────────────────────────────
    float contentY = panelY + Layout::Chat::kHeaderH;
    if (chat.replyTimerActive && chat.replyTimerMax > 0.0f) {
        float ratio = std::clamp(chat.replyTimer / chat.replyTimerMax, 0.0f, 1.0f);
        float barW = panelW * ratio;

        draw->AddRectFilled(
            ImVec2(panelX, contentY),
            ImVec2(panelX + barW, contentY + Layout::Chat::kTimerBarH),
            Col32_RatioColor(ratio));
        contentY += Layout::Chat::kTimerBarH + Layout::Chat::kTimerBarGap;
    }

    // ── Messages area ─────────────────────────────────────
    ImFont* bodyFont = GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    float msgStartY = contentY + Layout::Chat::kMsgTopPad;

    // Reply area layout
    float replyAreaH = Layout::Chat::kHintLineH;
    if (chat.replyCount > 0) {
        replyAreaH  = Layout::Chat::kReplyPad;
        if (chat.replyTimerActive) replyAreaH += Layout::Chat::kCountdownH;
        replyAreaH += Layout::Chat::kInputBoxH + Layout::Chat::kReplySpacing;
        replyAreaH += chat.replyCount * Layout::Chat::kReplyItemH;
        replyAreaH += Layout::Chat::kHintLineH;
    }

    float msgEndY = panelY + panelH - replyAreaH;

    // Calculate how many messages fit
    int maxVisible = (int)((msgEndY - msgStartY) / Layout::Chat::kMsgLineH);
    int startMsg = std::max(0, chat.messageCount - maxVisible);

    float msgY = msgStartY;
    for (int i = startMsg; i < chat.messageCount && i < Res_ChatState::kMaxMessages; ++i) {
        if (msgY > msgEndY) break;

        const auto& msg = chat.messages[i];

        // Sender color by type
        ImU32 senderColor;
        switch (msg.senderType) {
            case 1:  // Player
                senderColor = Col32_Accent(240);
                break;
            case 2:  // NPC
                senderColor = modeColor;
                break;
            default: // System
                senderColor = Col32_Gray(200);
                break;
        }

        // Sender tag
        char senderBuf[40];
        snprintf(senderBuf, sizeof(senderBuf), "[%s]", msg.sender);
        draw->AddText(ImVec2(panelX + Layout::Chat::kMsgPadX, msgY), senderColor, senderBuf);

        // Message text
        ImVec2 senderSize = ImGui::CalcTextSize(senderBuf);
        float textX = panelX + Layout::Chat::kMsgPadX + senderSize.x + Layout::Chat::kSenderGap;

        // Simple word-wrap: just truncate if too long for now
        draw->AddText(ImVec2(textX, msgY),
            Col32_Text(220), msg.text);
        msgY += Layout::Chat::kMsgLineH;
    }

    if (bodyFont) ImGui::PopFont();

    // ── Reply area separator ──────────────────────────────
    float replyTopY = panelY + panelH - replyAreaH;
    draw->AddLine(
        ImVec2(panelX + Layout::Chat::kSepInset, replyTopY),
        ImVec2(panelX + panelW - Layout::Chat::kSepInset, replyTopY),
        Col32_Gray(100), 1.0f);

    if (chat.replyCount > 0) {
        float curY = replyTopY + Layout::Chat::kReplyPad;
        ImFont* largeFont = GetFont_TerminalLarge();

        // ── Countdown number (centered, 32px) ────────────
        if (chat.replyTimerActive && chat.replyTimerMax > 0.0f) {
            int secs = (int)std::ceil(chat.replyTimer);
            char timerBuf[8];
            snprintf(timerBuf, sizeof(timerBuf), "%d", secs);

            if (largeFont) ImGui::PushFont(largeFont);
            ImVec2 timerSize = ImGui::CalcTextSize(timerBuf);
            float timerX = panelX + (panelW - timerSize.x) * 0.5f;

            float ratio = std::clamp(chat.replyTimer / chat.replyTimerMax, 0.0f, 1.0f);

            draw->AddText(ImVec2(timerX, curY), Col32_RatioColor(ratio), timerBuf);
            if (largeFont) ImGui::PopFont();
            curY += Layout::Chat::kCountdownH;
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
        float totalSlotW = Res_ChatState::kInputBufferSize * Layout::Chat::kSlotSize
                         + (Res_ChatState::kInputBufferSize - 1) * Layout::Chat::kSlotGap;
        float slotStartX = panelX + (panelW - totalSlotW) * 0.5f;

        for (int s = 0; s < Res_ChatState::kInputBufferSize; ++s) {
            float sx = slotStartX + s * (Layout::Chat::kSlotSize + Layout::Chat::kSlotGap);
            float sy = curY;

            if (s < chat.inputBufferLen) {
                // Filled slot — orange border + arrow
                draw->AddRect(ImVec2(sx, sy), ImVec2(sx + Layout::Chat::kSlotSize, sy + Layout::Chat::kSlotSize),
                    Col32_Accent(), 2.0f, 0, 2.0f);
                DrawArrow(sx + Layout::Chat::kSlotSize * 0.5f, sy + Layout::Chat::kSlotSize * 0.5f,
                    static_cast<int>(chat.inputBuffer[s]), Col32_Accent(), Layout::Chat::kArrowSize);
            } else {
                // Empty slot — gray dashed border
                draw->AddRect(ImVec2(sx, sy), ImVec2(sx + Layout::Chat::kSlotSize, sy + Layout::Chat::kSlotSize),
                    Col32_Gray(100), 2.0f, 0, 1.0f);
            }
        }
        curY += Layout::Chat::kInputBoxH;

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
        curY += Layout::Chat::kReplySpacing;
        for (int i = 0; i < chat.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
            bool isMatch = (i == matchedReply);

            // Highlight background for matched option
            if (isMatch) {
                draw->AddRectFilled(
                    ImVec2(panelX + Layout::Chat::kHighlightInset, curY - 2.0f),
                    ImVec2(panelX + panelW - Layout::Chat::kHighlightInset, curY + Layout::Chat::kHighlightH),
                    Col32_Accent(30), 2.0f);
            }

            // Text line
            ImU32 textColor = isMatch ? Col32_Accent()
                                      : Col32_Text(160);
            draw->AddText(ImVec2(panelX + Layout::Chat::kMsgPadX, curY), textColor, chat.replies[i].text);

            // Direction sequence arrows (below text)
            const auto& seq = chat.replySequences[i];
            float arrowY = curY + Layout::Chat::kArrowYOffset;
            float arrowX = panelX + Layout::Chat::kArrowX0;

            for (uint8_t k = 0; k < seq.length; ++k) {
                // Bright if matched so far, dim otherwise
                bool keyMatched = isMatch && (k < chat.inputBufferLen);
                ImU32 arrowColor = keyMatched ? Col32_Accent()
                                             : Col32_Text(80);
                DrawArrow(arrowX + Layout::Chat::kArrowCellW * 0.5f, arrowY + Layout::Chat::kArrowCenterY,
                    static_cast<int>(seq.keys[k]), arrowColor, Layout::Chat::kArrowSize);
                arrowX += Layout::Chat::kArrowCellW;
            }

            curY += Layout::Chat::kReplyItemH;
        }
        if (bodyFont) ImGui::PopFont();

        // ── Bottom hint ──────────────────────────────────
        if (smallFont) ImGui::PushFont(smallFont);
        const char* hintText = "[ARROW KEYS] INPUT";
        ImVec2 hintSize = ImGui::CalcTextSize(hintText);
        float hintX = panelX + (panelW - hintSize.x) * 0.5f;
        draw->AddText(ImVec2(hintX, panelY + panelH - Layout::Chat::kHintBottomY),
            Col32_Text(100), hintText);
        if (smallFont) ImGui::PopFont();
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
