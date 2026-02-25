#include "UI_GameOver.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// GameOver 静态数据
// ============================================================

static const char* kGameOverItems[] = {
    "RETRY",
    "RETURN TO MENU",
};
static constexpr int kGameOverItemCount = 2;

// ============================================================
// RenderGameOverScreen
// ============================================================

void RenderGameOverScreen(Registry& registry, float /*dt*/) {
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##GameOver", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // 半透明暗化背景
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(4, 6, 10, 220));

    // 面板居中
    float panelW = 450.0f;
    float panelH = 340.0f;
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // 面板背景
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(8, 12, 18, 245), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(0, 140, 130, 100), 3.0f, 0, 1.0f);

    // ── 根据 gameOverReason 确定标题和颜色 ──
    uint8_t reason = 0;
    float finalAlertLevel = 0.0f;
    float gameTime = 0.0f;

    if (registry.has_ctx<Res_GameplayState>()) {
        auto& gs = registry.ctx<Res_GameplayState>();
        reason = gs.gameOverReason;
        finalAlertLevel = gs.alertLevel;
        gameTime = gs.gameOverTime;
    }

    const char* titleText;
    ImU32 titleColor;
    const char* reasonText;

    switch (reason) {
        case 1:
            titleText  = "MISSION FAILED";
            titleColor = IM_COL32(220, 40, 30, 255);
            reasonText = "COUNTDOWN EXPIRED - SYSTEM LOCKDOWN";
            break;
        case 2:
            titleText  = "MISSION FAILED";
            titleColor = IM_COL32(220, 40, 30, 255);
            reasonText = "OPERATOR DETECTED - CONNECTION SEVERED";
            break;
        case 3:
            titleText  = "MISSION COMPLETE";
            titleColor = IM_COL32(0, 220, 210, 255);
            reasonText = "ALL OBJECTIVES ACHIEVED";
            break;
        default:
            titleText  = "MISSION TERMINATED";
            titleColor = IM_COL32(220, 140, 0, 255);
            reasonText = "OPERATION STATUS UNKNOWN";
            break;
    }

    // ── 标题 ──
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    ImVec2 titleSize = ImGui::CalcTextSize(titleText);
    float titleX = panelX + (panelW - titleSize.x) * 0.5f;
    float titleY = panelY + 25.0f;

    // 发光效果（半亮度底层 + 正常层）
    ImU32 glowColor = IM_COL32(
        ((titleColor >> IM_COL32_R_SHIFT) & 0xFF) >> 1,
        ((titleColor >> IM_COL32_G_SHIFT) & 0xFF) >> 1,
        ((titleColor >> IM_COL32_B_SHIFT) & 0xFF) >> 1, 60);
    draw->AddText(ImVec2(titleX, titleY), glowColor, titleText);
    draw->AddText(ImVec2(titleX, titleY), titleColor, titleText);

    if (titleFont) ImGui::PopFont();

    // ── 分隔线 ──
    float lineY = titleY + titleSize.y + 10.0f;
    draw->AddLine(
        ImVec2(panelX + 30.0f, lineY),
        ImVec2(panelX + panelW - 30.0f, lineY),
        IM_COL32(0, 140, 130, 80), 1.0f);

    // ── 失败/成功原因 ──
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    ImVec2 reasonSize = ImGui::CalcTextSize(reasonText);
    float reasonX = panelX + (panelW - reasonSize.x) * 0.5f;
    draw->AddText(ImVec2(reasonX, lineY + 12.0f),
        IM_COL32(150, 160, 170, 200), reasonText);

    if (termFont) ImGui::PopFont();

    // ── 统计摘要 ──
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    float statsY = lineY + 45.0f;
    float statsX = panelX + 50.0f;

    // 耗时
    int minutes = (int)(gameTime / 60.0f);
    int seconds = (int)(gameTime) % 60;
    char timeBuf[48];
    snprintf(timeBuf, sizeof(timeBuf), "ELAPSED TIME:     %02d:%02d", minutes, seconds);
    draw->AddText(ImVec2(statsX, statsY), IM_COL32(0, 180, 170, 200), timeBuf);

    // 最终警戒度
    char alertBuf[48];
    snprintf(alertBuf, sizeof(alertBuf), "FINAL ALERT:      %.0f / 150", finalAlertLevel);
    draw->AddText(ImVec2(statsX, statsY + 22.0f), IM_COL32(0, 180, 170, 200), alertBuf);

    // 评级
    const char* grade;
    if (reason == 3) {
        grade = (finalAlertLevel < 30.0f) ? "S" :
                (finalAlertLevel < 75.0f) ? "A" : "B";
    } else {
        grade = "F";
    }
    char gradeBuf[48];
    snprintf(gradeBuf, sizeof(gradeBuf), "RATING:           %s", grade);
    ImU32 gradeColor = (reason == 3) ? IM_COL32(0, 220, 210, 255)
                                     : IM_COL32(220, 40, 30, 220);
    draw->AddText(ImVec2(statsX, statsY + 44.0f), gradeColor, gradeBuf);

    if (smallFont) ImGui::PopFont();

    // ── 分隔线 ──
    float menuLineY = statsY + 80.0f;
    draw->AddLine(
        ImVec2(panelX + 30.0f, menuLineY),
        ImVec2(panelX + panelW - 30.0f, menuLineY),
        IM_COL32(0, 140, 130, 60), 1.0f);

    // ── 菜单选项 ──
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = menuLineY + 15.0f;
    float menuItemH  = 38.0f;

    // 键盘导航
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.gameOverSelectedIndex =
                (ui.gameOverSelectedIndex - 1 + kGameOverItemCount) % kGameOverItemCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.gameOverSelectedIndex =
                (ui.gameOverSelectedIndex + 1) % kGameOverItemCount;
        }
    }

    // 先处理鼠标 hover 更新 selectedIndex
    const Mouse* mouse = Window::GetMouse();
    for (int i = 0; i < kGameOverItemCount; ++i) {
        ImVec2 itemMin(panelX + 30.0f, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.gameOverSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    // 确认检测（统一在 hover 更新后）
    int8_t confirmedIndex = -1;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.gameOverSelectedIndex;
    }
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        confirmedIndex = ui.gameOverSelectedIndex;
    }

    for (int i = 0; i < kGameOverItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.gameOverSelectedIndex);

        float baseX = panelX + 50.0f + (isSelected ? 4.0f : 0.0f);

        ImVec2 itemMin(panelX + 30.0f, itemY - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, itemY + menuItemH - 6.0f);

        // 高亮
        if (isSelected) {
            draw->AddRectFilled(itemMin, itemMax, IM_COL32(0, 80, 75, 40), 2.0f);
            draw->AddRect(itemMin, itemMax, IM_COL32(0, 180, 170, 60), 2.0f, 0, 1.0f);
        }

        // 文字
        char buf[64];
        snprintf(buf, sizeof(buf), isSelected ? "> %s" : "  %s", kGameOverItems[i]);
        ImU32 textColor = isSelected ? IM_COL32(0, 220, 210, 255)
                                     : IM_COL32(130, 140, 150, 220);
        draw->AddText(ImVec2(baseX, itemY), textColor, buf);
    }

    if (termFont) ImGui::PopFont();

    // 确认操作
    if (confirmedIndex >= 0) {
        switch (confirmedIndex) {
            case 0: // RETRY
                ui.pendingSceneRequest = SceneRequest::RestartLevel;
                LOG_INFO("[UI_GameOver] GameOver -> RestartLevel");
                break;
            case 1: // RETURN TO MENU
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[UI_GameOver] GameOver -> ReturnToMenu");
                break;
            default:
                break;
        }
    }

    // 底部提示
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(
        ImVec2(panelX + 30.0f, panelY + panelH - 30.0f),
        IM_COL32(60, 70, 75, 150),
        "[W/S] NAVIGATE  [ENTER] SELECT");

    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
