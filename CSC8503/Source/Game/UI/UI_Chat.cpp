/**
 * @file UI_Chat.cpp
 * @brief 聊天面板渲染实现（消息列表 + 方向键回复 UI + 倒计时）。
 */
#include "UI_Chat.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cfloat>
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
    if (!bodyFont)  bodyFont  = ImGui::GetFont();
    if (!smallFont) smallFont = ImGui::GetFont();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const float bodyFontSz  = bodyFont->LegacySize;
    const float smallFontSz = smallFont->LegacySize;
    float msgStartY = contentY + Layout::Chat::kMsgTopPad;
    const float wrapW = panelW - Layout::Chat::kMsgPadX * 2.0f;

    // Pre-compute per-reply-item height (for dynamic reply area)
    float replyItemH[Res_ChatState::kMaxReplies] = {};
    float totalReplyItemsH = 0.0f;
    for (int i = 0; i < chat.replyCount; ++i) {
        ImVec2 sz = bodyFont->CalcTextSizeA(
            bodyFontSz, FLT_MAX, wrapW, chat.replies[i].text);
        float textH = std::max(sz.y, Layout::Chat::kReplyTextMinH);
        replyItemH[i] = Layout::Chat::kReplyItemPadY + textH
                       + Layout::Chat::kReplyItemPadY + Layout::Chat::kReplyArrowH
                       + Layout::Chat::kReplyGap;
        totalReplyItemsH += replyItemH[i];
    }

    // Reply area layout (dynamic height)
    float replyAreaH = Layout::Chat::kHintLineH;
    if (chat.replyCount > 0) {
        replyAreaH  = Layout::Chat::kReplyPad;
        if (chat.replyTimerActive) replyAreaH += Layout::Chat::kCountdownH;
        replyAreaH += Layout::Chat::kInputBoxH + Layout::Chat::kReplySpacing;
        replyAreaH += totalReplyItemsH;
        replyAreaH += Layout::Chat::kHintLineH;
    }

    float msgEndY = panelY + panelH - replyAreaH;

    // Pre-compute per-message height (sender line + wrapped text)
    float msgHeights[Res_ChatState::kMaxMessages] = {};
    for (int i = 0; i < chat.messageCount; ++i) {
        float h = Layout::Chat::kSenderLineH + Layout::Chat::kSenderMsgGap;
        ImVec2 textSz = bodyFont->CalcTextSizeA(
            bodyFontSz, FLT_MAX, wrapW,
            chat.messages[i].text);
        h += std::max(textSz.y, Layout::Chat::kMsgMinTextH);
        h += Layout::Chat::kMsgBottomPad;
        msgHeights[i] = h;
    }

    // Bottom-anchored scroll: find first visible message
    float availH = msgEndY - msgStartY;
    float accum = 0.0f;
    int startMsg = chat.messageCount;
    for (int i = chat.messageCount - 1; i >= 0; --i) {
        if (accum + msgHeights[i] > availH) break;
        accum += msgHeights[i];
        startMsg = i;
    }

    // Render messages with clip rect
    draw->PushClipRect(ImVec2(panelX, msgStartY), ImVec2(panelX + panelW, msgEndY), true);
    float msgY = msgStartY;
    for (int i = startMsg; i < chat.messageCount; ++i) {
        if (msgY >= msgEndY) break;

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

        // Sender tag (small font, independent line)
        char senderBuf[40];
        snprintf(senderBuf, sizeof(senderBuf), "[%s]", msg.sender);
        draw->AddText(smallFont, smallFontSz,
            ImVec2(panelX + Layout::Chat::kMsgPadX, msgY), senderColor, senderBuf);
        msgY += Layout::Chat::kSenderLineH + Layout::Chat::kSenderMsgGap;

        // Message body (wrapped text, full panel width)
        draw->AddText(bodyFont, bodyFontSz,
            ImVec2(panelX + Layout::Chat::kMsgPadX, msgY),
            Col32_Text(220), msg.text, nullptr, wrapW);
        ImVec2 sz = bodyFont->CalcTextSizeA(bodyFontSz, FLT_MAX, wrapW, msg.text);
        msgY += std::max(sz.y, Layout::Chat::kMsgMinTextH) + Layout::Chat::kMsgBottomPad;
    }
    draw->PopClipRect();

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

        // ── Reply options (wrapped text + direction sequence) ─
        if (bodyFont) ImGui::PushFont(bodyFont);
        curY += Layout::Chat::kReplySpacing;
        for (int i = 0; i < chat.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
            bool isMatch = (i == matchedReply);
            float itemH = replyItemH[i] - Layout::Chat::kReplyGap;

            // Highlight background for matched option (dynamic height)
            if (isMatch) {
                draw->AddRectFilled(
                    ImVec2(panelX + Layout::Chat::kHighlightInset, curY),
                    ImVec2(panelX + panelW - Layout::Chat::kHighlightInset, curY + itemH),
                    Col32_Accent(30), 2.0f);
            }

            // Reply text (wrapped)
            ImU32 textColor = isMatch ? Col32_Accent()
                                      : Col32_Text(160);
            float textY = curY + Layout::Chat::kReplyItemPadY;
            draw->AddText(bodyFont, bodyFontSz,
                ImVec2(panelX + Layout::Chat::kMsgPadX, textY),
                textColor, chat.replies[i].text, nullptr, wrapW);
            ImVec2 sz = bodyFont->CalcTextSizeA(
                bodyFontSz, FLT_MAX, wrapW, chat.replies[i].text);
            float textH = std::max(sz.y, Layout::Chat::kReplyTextMinH);

            // Arrows below text
            float arrowY = textY + textH + Layout::Chat::kReplyItemPadY;
            float arrowX = panelX + Layout::Chat::kArrowX0;
            const auto& seq = chat.replySequences[i];

            for (uint8_t k = 0; k < seq.length; ++k) {
                bool keyMatched = isMatch && (k < chat.inputBufferLen);
                ImU32 arrowColor = keyMatched ? Col32_Accent()
                                             : Col32_Text(80);
                DrawArrow(arrowX + Layout::Chat::kArrowCellW * 0.5f, arrowY + Layout::Chat::kArrowCenterY,
                    static_cast<int>(seq.keys[k]), arrowColor, Layout::Chat::kArrowSize);
                arrowX += Layout::Chat::kArrowCellW;
            }

            curY += replyItemH[i];
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
