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
// RenderChatPanel — Right-side terminal-style chat
// ============================================================

void RenderChatPanel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_ChatState>()) return;
    auto& chat = registry.ctx<Res_ChatState>();

    if (!chat.chatOpen) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Panel dimensions — right side
    constexpr float kPanelW = 320.0f;
    constexpr float kMargin = 10.0f;
    float panelX = displaySize.x - kPanelW - kMargin;
    float panelY = kMargin + 60.0f; // Below score display
    float panelH = displaySize.y - panelY - kMargin - 60.0f;

    // Panel background
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + kPanelW, panelY + panelH),
        IM_COL32(245, 238, 232, 230), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + kPanelW, panelY + panelH),
        IM_COL32(200, 200, 200, 150), 3.0f);

    // Header
    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(panelX + 10.0f, panelY + 8.0f),
        IM_COL32(252, 111, 41, 220), "COMMS TERMINAL");
    if (termFont) ImGui::PopFont();

    float headerLineY = panelY + 30.0f;
    draw->AddLine(
        ImVec2(panelX + 8.0f, headerLineY),
        ImVec2(panelX + kPanelW - 8.0f, headerLineY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // Messages area
    if (smallFont) ImGui::PushFont(smallFont);

    float msgY = headerLineY + 8.0f;
    float msgEndY = panelY + panelH - 40.0f;
    constexpr float kMsgLineH = 16.0f;

    int startMsg = std::max(0, chat.messageCount - (int)((msgEndY - msgY) / kMsgLineH));
    for (int i = startMsg; i < chat.messageCount && i < Res_ChatState::kMaxMessages; ++i) {
        if (msgY > msgEndY) break;

        const auto& msg = chat.messages[i];
        ImU32 senderColor = msg.isSystem
            ? IM_COL32(252, 111, 41, 200)
            : IM_COL32(16, 13, 10, 220);

        // Sender
        char line[196];
        snprintf(line, sizeof(line), "[%s] %s", msg.sender, msg.text);
        draw->AddText(ImVec2(panelX + 10.0f, msgY), senderColor, line);
        msgY += kMsgLineH;
    }

    if (smallFont) ImGui::PopFont();

    // Input area separator
    float inputLineY = panelY + panelH - 35.0f;
    draw->AddLine(
        ImVec2(panelX + 8.0f, inputLineY),
        ImVec2(panelX + kPanelW - 8.0f, inputLineY),
        IM_COL32(200, 200, 200, 80), 1.0f);

    // Reply options (if any)
    if (chat.replyCount > 0 && smallFont) {
        ImGui::PushFont(smallFont);
        float replyY = inputLineY + 6.0f;
        for (int i = 0; i < chat.replyCount && i < Res_ChatState::kMaxReplies; ++i) {
            bool isSel = (i == chat.selectedReply);
            ImU32 replyColor = isSel ? IM_COL32(252, 111, 41, 255)
                                     : IM_COL32(16, 13, 10, 160);
            char replyBuf[72];
            snprintf(replyBuf, sizeof(replyBuf), isSel ? "> %s" : "  %s",
                chat.replies[i].text);
            draw->AddText(ImVec2(panelX + 10.0f, replyY),
                replyColor, replyBuf);
            replyY += kMsgLineH;
        }
        ImGui::PopFont();
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
