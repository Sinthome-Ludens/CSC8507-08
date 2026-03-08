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

// ============================================================
// RenderChatPanel — 常驻右侧面板 (320px, 全高)
// ============================================================

void RenderChatPanel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_ChatState>()) return;
    auto& chat = registry.ctx<Res_ChatState>();

    // 常驻面板：HUD 状态下始终渲染（chatOpen 由 Sys_UI 管理）
    if (!chat.chatOpen) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Panel dimensions — right side, full height
    float panelW = Res_ChatState::PANEL_WIDTH;
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
    if (smallFont) ImGui::PushFont(smallFont);

    float msgStartY = contentY + 6.0f;
    float replyAreaH = (chat.replyCount > 0) ? (chat.replyCount * 18.0f + 30.0f) : 24.0f;
    float msgEndY = panelY + panelH - replyAreaH;
    constexpr float kMsgLineH = 18.0f;

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
        float availW = panelW - (textX - panelX) - 8.0f;

        // Simple word-wrap: just truncate if too long for now
        draw->AddText(ImVec2(textX, msgY),
            IM_COL32(16, 13, 10, 220), msg.text);
        msgY += kMsgLineH;
    }

    if (smallFont) ImGui::PopFont();

    // ── Reply area separator ──────────────────────────────
    float inputLineY = panelY + panelH - replyAreaH;
    draw->AddLine(
        ImVec2(panelX + 8.0f, inputLineY),
        ImVec2(panelX + panelW - 8.0f, inputLineY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // ── Reply options ─────────────────────────────────────
    if (chat.replyCount > 0 && smallFont) {
        ImGui::PushFont(smallFont);
        float replyY = inputLineY + 6.0f;
        for (int i = 0; i < chat.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
            bool isSel = (i == chat.selectedReply);

            // Selection highlight
            if (isSel) {
                draw->AddRectFilled(
                    ImVec2(panelX + 6.0f, replyY - 1.0f),
                    ImVec2(panelX + panelW - 6.0f, replyY + 15.0f),
                    IM_COL32(252, 111, 41, 30), 2.0f);
            }

            // Reply number + text
            ImU32 replyColor = isSel ? IM_COL32(252, 111, 41, 255)
                                     : IM_COL32(16, 13, 10, 160);
            char replyBuf[72];
            snprintf(replyBuf, sizeof(replyBuf), "[%d] %s", i + 1, chat.replies[i].text);
            draw->AddText(ImVec2(panelX + 10.0f, replyY), replyColor, replyBuf);
            replyY += 18.0f;
        }
        ImGui::PopFont();
    }

    // ── Bottom hint ───────────────────────────────────────
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(panelX + 10.0f, panelY + panelH - 18.0f),
        IM_COL32(16, 13, 10, 100),
        "[1-4] REPLY");
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
