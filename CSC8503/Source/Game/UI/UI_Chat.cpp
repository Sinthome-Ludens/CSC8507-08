#include "UI_Chat.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// 辅助：chatMode 对应的主题色
// ============================================================

static ImU32 GetModeColor(uint8_t chatMode, uint8_t alpha = 255) {
    switch (chatMode) {
        case 0:  return IM_COL32(0, 200, 180, alpha);    // proactive: 青绿
        case 1:  return IM_COL32(220, 200, 0, alpha);    // mixed: 黄色
        case 2:  return IM_COL32(220, 40, 30, alpha);    // passive: 红色
        default: return IM_COL32(0, 200, 180, alpha);
    }
}

static const char* GetModeLabel(uint8_t chatMode) {
    switch (chatMode) {
        case 0:  return "SECURE";
        case 1:  return "ALERT";
        case 2:  return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// ============================================================
// RenderChatPanel — 右侧聊天面板
// ============================================================

void RenderChatPanel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_ChatState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;

    auto& chat = registry.ctx<Res_ChatState>();
    auto& ui   = registry.ctx<Res_UIState>();

    // 聊天面板始终可见（左右分屏布局，占据右侧区域）

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    // 面板尺寸和位置（右侧，全高，贴右边缘）
    const float panelW = Res_ChatState::PANEL_WIDTH;
    const float panelH = vpSize.y;
    const float panelX = vpPos.x + vpSize.x - panelW;
    const float panelY = vpPos.y;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    ImU32 modeColor = GetModeColor(chat.chatMode);
    ImU32 modeDim   = GetModeColor(chat.chatMode, 80);

    // ── 面板背景（完全不透明，独立区域）──
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(6, 8, 14, 255));
    // 左侧分隔线
    draw->AddLine(
        ImVec2(panelX, panelY),
        ImVec2(panelX, panelY + panelH),
        modeDim, 2.0f);

    // ── 标题栏 ──
    float headerH = 30.0f;
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + headerH),
        IM_COL32(10, 14, 20, 240), 3.0f);

    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    // 标题文字
    draw->AddText(ImVec2(panelX + 8.0f, panelY + 7.0f),
        modeColor, "TERMINAL // FIREWALL ADMIN");

    // 模式标签（右上角）
    const char* modeLabel = GetModeLabel(chat.chatMode);
    ImVec2 labelSize = ImGui::CalcTextSize(modeLabel);
    draw->AddText(
        ImVec2(panelX + panelW - labelSize.x - 8.0f, panelY + 7.0f),
        modeColor, modeLabel);

    if (smallFont) ImGui::PopFont();

    // ── 分隔线 ──
    draw->AddLine(
        ImVec2(panelX + 5.0f, panelY + headerH),
        ImVec2(panelX + panelW - 5.0f, panelY + headerH),
        modeDim, 1.0f);

    // ── 消息区域 ──
    float msgAreaTop = panelY + headerH + 5.0f;
    float msgAreaBottom = panelY + panelH - 160.0f;  // 给回复区域留空间
    float msgY = msgAreaBottom;  // 从底部向上排列

    ImFont* termFont = UITheme::GetFont_Terminal();

    if (termFont) ImGui::PushFont(termFont);

    // 从最新消息开始向上渲染
    float lineH = 18.0f;

    for (int i = chat.messageCount - 1; i >= 0 && msgY > msgAreaTop; --i) {
        const auto& msg = chat.GetMessage(i);
        if (!msg.used) continue;

        msgY -= lineH;

        ImU32 msgColor;
        switch (msg.sender) {
            case 0:  // system
                msgColor = IM_COL32(80, 90, 100, 180);
                break;
            case 1:  // player
                msgColor = IM_COL32(0, 220, 210, 230);
                break;
            case 2:  // admin (NPC)
                msgColor = modeColor;
                break;
            default:
                msgColor = IM_COL32(100, 110, 120, 180);
                break;
        }

        draw->AddText(ImVec2(panelX + 8.0f, msgY), msgColor, msg.text);
    }

    if (termFont) ImGui::PopFont();

    // ── 分隔线（消息区和回复区之间）──
    float replyDivY = panelY + panelH - 155.0f;
    draw->AddLine(
        ImVec2(panelX + 5.0f, replyDivY),
        ImVec2(panelX + panelW - 5.0f, replyDivY),
        modeDim, 1.0f);

    // ── 回复计时条（限时回复时显示）──
    if (chat.replyTimerActive && chat.waitingForReply) {
        float barX = panelX + 8.0f;
        float barY = replyDivY + 5.0f;
        float barW = panelW - 16.0f;
        float barH = 6.0f;

        float ratio = (chat.replyTimerMax > 0.0f) ? chat.replyTimer / chat.replyTimerMax : 0.0f;
        if (ratio < 0.0f) ratio = 0.0f;

        // 背景条
        draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
            IM_COL32(20, 25, 30, 200), 2.0f);

        // 填充条（颜色随剩余时间变化）
        ImU32 barColor;
        if (ratio > 0.5f)      barColor = IM_COL32(0, 200, 180, 220);
        else if (ratio > 0.25f) barColor = IM_COL32(220, 200, 0, 220);
        else                    barColor = IM_COL32(220, 40, 30, 255);

        float fillW = barW * ratio;
        if (fillW > 0.0f) {
            draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH),
                barColor, 2.0f);
        }

        // 时间文字
        if (smallFont) ImGui::PushFont(smallFont);
        char timerBuf[16];
        snprintf(timerBuf, sizeof(timerBuf), "%.1fs", chat.replyTimer);
        ImVec2 timerSize = ImGui::CalcTextSize(timerBuf);
        draw->AddText(ImVec2(barX + barW - timerSize.x, barY + barH + 1.0f),
            barColor, timerBuf);
        if (smallFont) ImGui::PopFont();
    }

    // ── 回复选项 ──
    if (chat.waitingForReply && chat.replyCount > 0) {
        float replyStartY = replyDivY + 22.0f;
        float replyItemH  = 28.0f;

        if (termFont) ImGui::PushFont(termFont);

        for (uint8_t i = 0; i < chat.replyCount; ++i) {
            float itemY = replyStartY + i * replyItemH;
            bool isSelected = (i == chat.selectedReply);

            ImVec2 itemMin(panelX + 6.0f, itemY - 1.0f);
            ImVec2 itemMax(panelX + panelW - 6.0f, itemY + replyItemH - 5.0f);

            // 鼠标检测
            ImVec2 mousePos = ImGui::GetMousePos();
            bool hover = (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                          mousePos.y >= itemMin.y && mousePos.y <= itemMax.y);
            if (hover) {
                chat.selectedReply = static_cast<int8_t>(i);
            }

            // 高亮选中
            if (isSelected) {
                draw->AddRectFilled(itemMin, itemMax, IM_COL32(0, 80, 75, 40), 2.0f);
                draw->AddRect(itemMin, itemMax, modeDim, 2.0f, 0, 1.0f);
            }

            // 回复文字
            char replyBuf[56];
            snprintf(replyBuf, sizeof(replyBuf), "%d> %s",
                     i + 1, chat.replies[i].text);

            ImU32 replyColor = isSelected ? IM_COL32(0, 220, 210, 255)
                                          : IM_COL32(100, 110, 120, 200);
            draw->AddText(ImVec2(panelX + 12.0f, itemY + 2.0f), replyColor, replyBuf);
        }

        if (termFont) ImGui::PopFont();
    } else if (!chat.waitingForReply) {
        // 等待NPC消息提示
        if (smallFont) ImGui::PushFont(smallFont);

        float waitY = replyDivY + 30.0f;
        float blink = (sinf(ui.globalTime * UITheme::kPI * 2.0f) + 1.0f) * 0.5f;
        uint8_t blinkAlpha = (uint8_t)(80.0f + blink * 100.0f);

        draw->AddText(ImVec2(panelX + 8.0f, waitY),
            IM_COL32(80, 90, 100, blinkAlpha),
            "Waiting for admin response...");

        if (smallFont) ImGui::PopFont();
    }

    // ── 底部提示 ──
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(
        ImVec2(panelX + 8.0f, panelY + panelH - 18.0f),
        IM_COL32(50, 55, 60, 150),
        "[1-4] Reply");

    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
