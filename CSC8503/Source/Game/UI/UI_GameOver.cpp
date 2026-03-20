/**
 * @file UI_GameOver.cpp
 * @brief 游戏结束画面 UI。
 */
#include "UI_GameOver.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Anim.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_UIKeyConfig.h"
#include "Keyboard.h"
#include "Mouse.h"

using namespace NCL;
using namespace ECS::UITheme;

namespace ECS::UI {

// ============================================================
// RenderGameOverScreen
// ============================================================

static const char* kGameOverItems[] = {
    "RETRY",
    "RETURN TO MENU",
};
static constexpr int kGameOverItemCount = 2;

/**
 * @brief 渲染游戏结束界面，并根据输入写回场景切换请求。
 * @details 读取 `Res_UIState` 与 `Res_GameState` 的单机/多人结果状态，
 *          决定标题、统计信息以及 Retry/ReturnToMenu 的后续流程。
 * @param registry 当前 ECS 注册表
 * @param dt 本帧时间步长（未使用）
 */
void RenderGameOverScreen(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    const auto& input = registry.ctx<Res_Input>();

    Res_UIKeyConfig defaultUiCfg;
    const auto& uiCfg = registry.has_ctx<Res_UIKeyConfig>() ? registry.ctx<Res_UIKeyConfig>() : defaultUiCfg;

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

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg());

    // Entry animation
    float entryRaw = (ui.screenEntryDuration > 0.0f)
        ? std::clamp(ui.screenEntryElapsed / ui.screenEntryDuration, 0.0f, 1.0f) : 1.0f;
    float entryT = Anim::EaseOutCubic(entryRaw);

    // Title drops from above: offset decreases 30→0
    float titleDropOffset = -30.0f * (1.0f - entryT);
    // Slide transition offset
    float slideX = Anim::SlideOffset(entryT, ui.transDirection);
    // Data fades in with slight delay (use entryT clamped to later portion)
    float dataFadeT = std::clamp((entryRaw - 0.3f) / 0.7f, 0.0f, 1.0f);

    float cx = vpPos.x + vpSize.x * 0.5f + slideX;

    // Get game data
    GameOverReason reason = GameOverReason::None;
    float alertLevel = 0.0f;
    float alertMax = 150.0f;
    float playTime = 0.0f;
    bool isMultiplayer = false;
    MatchResult matchResult = MatchResult::None;
    if (registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();
        reason     = gs.gameOverReason;
        alertLevel = gs.alertLevel;
        alertMax   = gs.alertMax;
        playTime   = gs.playTime;
        isMultiplayer = gs.isMultiplayer;
        matchResult = gs.matchResult;
    }

    // ── Dynamic title based on reason ──────────────────────
    const char* resultTitle;
    const char* resultSubtitle;
    ImU32 titleColor;
    bool isSuccess = false;

    switch (reason) {
        case GameOverReason::Countdown:
            resultTitle    = "MISSION FAILED";
            resultSubtitle = "COUNTDOWN EXPIRED";
            titleColor     = IM_COL32(220, 60, 40, 255);
            break;
        case GameOverReason::Detected:
            resultTitle    = "MISSION FAILED";
            resultSubtitle = "OPERATOR DETECTED";
            titleColor     = IM_COL32(220, 60, 40, 255);
            break;
        case GameOverReason::Success:
            resultTitle    = "MISSION COMPLETE";
            resultSubtitle = "ALL OBJECTIVES ACHIEVED";
            titleColor     = Col32_Accent();
            isSuccess      = true;
            break;
        default:
            resultTitle    = "MISSION TERMINATED";
            resultSubtitle = "";
            titleColor     = Col32_Gray();
            break;
    }

    if (isMultiplayer) {
        switch (matchResult) {
            case MatchResult::LocalWin:
                resultSubtitle = "ALL OBJECTIVES ACHIEVED";
                break;
            case MatchResult::OpponentWin:
                resultSubtitle = "OPPONENT CLEARED THE FINAL STAGE";
                break;
            case MatchResult::Draw:
                resultTitle    = "MISSION TERMINATED";
                resultSubtitle = "BOTH OPERATIVES FINISHED";
                titleColor     = IM_COL32(180, 140, 40, 255);
                isSuccess      = false;
                break;
            case MatchResult::Disconnected:
                resultTitle    = "MISSION TERMINATED";
                resultSubtitle = "PEER DISCONNECTED";
                titleColor     = IM_COL32(120, 120, 120, 255);
                isSuccess      = false;
                break;
            case MatchResult::None:
            default:
                break;
        }
    }

    // ── Score + Rating ──────────────────────────────────────
    const int32_t finalScore = ui.campaignScore;
    const char* rating = GetScoreRating(finalScore);
    int8_t ratingTier = GetScoreRatingTier(finalScore);

    // 单人通关但积分≤500 → 覆盖为失败（在绘制标题之前判断）
    if (!isMultiplayer && isSuccess && finalScore <= 500) {
        resultTitle    = "MISSION FAILED";
        resultSubtitle = "INSUFFICIENT SCORE";
        titleColor     = IM_COL32(220, 60, 40, 255);
        isSuccess      = false;
    }

    // Title (drop-in animation)
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 titleSize = ImGui::CalcTextSize(resultTitle);
    float titleX = cx - titleSize.x * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.15f + titleDropOffset;
    uint8_t titleAlpha = (uint8_t)(entryT * 255);
    // Modulate title color alpha
    ImU32 animTitleColor = (titleColor & 0x00FFFFFFu) | ((ImU32)titleAlpha << 24);
    draw->AddText(ImVec2(titleX, titleY), animTitleColor, resultTitle);
    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);
    ImVec2 subSize = ImGui::CalcTextSize(resultSubtitle);
    draw->AddText(ImVec2(cx - subSize.x * 0.5f, titleY + titleSize.y + 6.0f),
        Col32_Text(180), resultSubtitle);
    if (termFont) ImGui::PopFont();

    // Decorative line
    float lineY = titleY + titleSize.y + 32.0f;
    draw->AddLine(ImVec2(cx - 120.0f, lineY), ImVec2(cx + 120.0f, lineY),
        Col32_Gray(120), 1.0f);

    const ImU32 ratingCol = GetScoreRatingColor(ratingTier);

    ImFont* bodyFont = GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);
    char ratingStr[32];
    snprintf(ratingStr, sizeof(ratingStr), "RATING: [%s]", rating);
    ImVec2 ratingSize = ImGui::CalcTextSize(ratingStr);
    float ratingY = lineY + 16.0f;
    draw->AddText(ImVec2(cx - ratingSize.x * 0.5f, ratingY), ratingCol, ratingStr);
    if (bodyFont) ImGui::PopFont();

    // ── Score Breakdown ──────────────────────────────────
    if (termFont) ImGui::PushFont(termFont);

    float statsY = ratingY + 46.0f;
    float statsX = cx - 140.0f;
    float valX   = cx + 40.0f;  // 右对齐值列
    char buf[64];
    ImU32 labelCol  = Col32_Text(220);
    ImU32 deductCol = IM_COL32(220, 60, 40, 220);

    // Play time MM:SS
    int totalSec = (int)playTime;
    int mm = totalSec / 60;
    int ss = totalSec % 60;

    snprintf(buf, sizeof(buf), "TIME:        %02d:%02d", mm, ss);
    draw->AddText(ImVec2(statsX, statsY), labelCol, buf);

    // INITIAL
    draw->AddText(ImVec2(statsX, statsY + 28.0f), labelCol, "INITIAL:");
    draw->AddText(ImVec2(valX, statsY + 28.0f), IM_COL32(80, 200, 120, 220), "1000");

    // TIME penalty
    snprintf(buf, sizeof(buf), "TIME (-1/s):");
    draw->AddText(ImVec2(statsX, statsY + 52.0f), labelCol, buf);
    if (ui.scoreLost_time > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_time);
        draw->AddText(ImVec2(valX, statsY + 52.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, statsY + 52.0f), labelCol, "0");
    }

    // KILLS
    snprintf(buf, sizeof(buf), "KILLS (x%d):", ui.scoreKillCount);
    draw->AddText(ImVec2(statsX, statsY + 76.0f), labelCol, buf);
    if (ui.scoreLost_kills > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_kills);
        draw->AddText(ImVec2(valX, statsY + 76.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, statsY + 76.0f), labelCol, "0");
    }

    // ITEMS
    snprintf(buf, sizeof(buf), "ITEMS (x%d):", ui.scoreItemUseCount);
    draw->AddText(ImVec2(statsX, statsY + 100.0f), labelCol, buf);
    if (ui.scoreLost_items > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_items);
        draw->AddText(ImVec2(valX, statsY + 100.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, statsY + 100.0f), labelCol, "0");
    }

    // COUNTDOWN
    draw->AddText(ImVec2(statsX, statsY + 124.0f), labelCol, "COUNTDOWN:");
    if (ui.scoreLost_countdown > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_countdown);
        draw->AddText(ImVec2(valX, statsY + 124.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, statsY + 124.0f), labelCol, "0");
    }

    // FAILURE
    draw->AddText(ImVec2(statsX, statsY + 148.0f), labelCol, "FAILURE:");
    if (ui.scoreLost_failure > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_failure);
        draw->AddText(ImVec2(valX, statsY + 148.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, statsY + 148.0f), labelCol, "0");
    }

    // Separator line
    draw->AddLine(ImVec2(statsX, statsY + 172.0f), ImVec2(valX + 60.0f, statsY + 172.0f),
        Col32_Gray(120), 1.0f);

    // FINAL SCORE
    snprintf(buf, sizeof(buf), "FINAL SCORE:");
    draw->AddText(ImVec2(statsX, statsY + 180.0f), labelCol, buf);
    snprintf(buf, sizeof(buf), "%d  [%s]", std::max(0, finalScore), rating);
    draw->AddText(ImVec2(valX, statsY + 180.0f), ratingCol, buf);

    if (termFont) ImGui::PopFont();

    // ── Separator before menu ─────────────────────────────
    float sepY = statsY + 210.0f;
    draw->AddLine(ImVec2(cx - 80.0f, sepY), ImVec2(cx + 80.0f, sepY),
        Col32_Gray(100), 1.0f);

    // ── Menu items ────────────────────────────────────────
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = sepY + 20.0f;
    float menuItemH  = 38.0f;

    {
        if (input.keyPressed[uiCfg.keyMenuUp] || input.keyPressed[uiCfg.keyMenuUpAlt]) {
            ui.gameOverSelectedIndex = (ui.gameOverSelectedIndex - 1 + kGameOverItemCount) % kGameOverItemCount;
        }
        if (input.keyPressed[uiCfg.keyMenuDown] || input.keyPressed[uiCfg.keyMenuDownAlt]) {
            ui.gameOverSelectedIndex = (ui.gameOverSelectedIndex + 1) % kGameOverItemCount;
        }
    }

    float itemW = 240.0f;
    float itemStartX = cx - itemW * 0.5f;

    for (int i = 0; i < kGameOverItemCount; ++i) {
        ImVec2 itemMin(itemStartX, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(itemStartX + itemW, menuStartY + i * menuItemH + menuItemH - 6.0f);
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.gameOverSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    int8_t confirmedIndex = -1;
    if (input.keyPressed[uiCfg.keyConfirm] || input.keyPressed[uiCfg.keyConfirmAlt]) {
        confirmedIndex = ui.gameOverSelectedIndex;
    }
    if (input.mouseButtonPressed[uiCfg.mouseConfirm]) {
        ImVec2 mousePos = ImGui::GetMousePos();
        for (int i = 0; i < kGameOverItemCount; ++i) {
            ImVec2 itemMin(itemStartX, menuStartY + i * menuItemH - 2.0f);
            ImVec2 itemMax(itemStartX + itemW, menuStartY + i * menuItemH + menuItemH - 6.0f);
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                confirmedIndex = static_cast<int8_t>(i);
                break;
            }
        }
    }

    // Update hover progress
    for (int i = 0; i < kGameOverItemCount; ++i) {
        float target = (i == ui.gameOverSelectedIndex) ? 1.0f : 0.0f;
        ui.menuHoverProgress[i] = Anim::SmoothLerp(ui.menuHoverProgress[i], target, 10.0f, dt);
    }

    for (int i = 0; i < kGameOverItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        float hoverT = Anim::EaseOutCubic(ui.menuHoverProgress[i]);

        ImVec2 itemMin(itemStartX, itemY - 2.0f);
        ImVec2 itemMax(itemStartX + itemW, itemY + menuItemH - 6.0f);

        uint8_t bgAlpha = (uint8_t)(hoverT * 35);
        uint8_t borderAlpha = (uint8_t)(80 + hoverT * 100);
        if (bgAlpha > 2) {
            draw->AddRectFilled(itemMin, itemMax, Col32_Accent(bgAlpha), 2.0f);
            draw->AddRect(itemMin, itemMax, Col32_Accent(borderAlpha), 2.0f, 0, 1.0f);
        }

        // Left orange bar
        if (hoverT > 0.01f) {
            float barFullH = menuItemH - 8.0f;
            float barH = hoverT * barFullH;
            float barCY = itemY + (menuItemH - 6.0f) * 0.5f - 2.0f;
            draw->AddRectFilled(
                ImVec2(itemMin.x, barCY - barH * 0.5f),
                ImVec2(itemMin.x + 3.0f, barCY + barH * 0.5f),
                Col32_Accent((uint8_t)(hoverT * 200)));
        }

        char label[64];
        bool showCaret = (hoverT > 0.5f);
        snprintf(label, sizeof(label), showCaret ? "> %s" : "  %s", kGameOverItems[i]);
        uint8_t textAlpha = (uint8_t)(220 + hoverT * 35);
        float textX = itemStartX + 10.0f + hoverT * 8.0f;
        draw->AddText(ImVec2(textX, itemY), Col32_Text(textAlpha), label);

        // Click flash
        if (ui.menuClickFlashIndex == i && ui.menuClickFlashTimer > 0.0f) {
            float flashT = ui.menuClickFlashTimer / 0.15f;
            uint8_t flashAlpha = (uint8_t)(180.0f * flashT);
            draw->AddRectFilled(itemMin, itemMax,
                IM_COL32(255, 255, 255, flashAlpha), 2.0f);
        }
    }

    if (termFont) ImGui::PopFont();

    // Confirm action
    if (confirmedIndex >= 0) {
        ui.menuClickFlashTimer = 0.15f;
        ui.menuClickFlashIndex = confirmedIndex;

        switch (confirmedIndex) {
            case 0: // RETRY
                if (isMultiplayer && matchResult != MatchResult::Disconnected) {
                    ui.multiplayerRetryRequested = true;
                } else {
                    ui.pendingSceneRequest = SceneRequest::RestartLevel;
                }
                LOG_INFO("[UI_GameOver] GameOver -> Retry");
                break;
            case 1: // RETURN TO MENU
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[UI_GameOver] GameOver -> ReturnToMenu");
                break;
            default:
                break;
        }
    }

    // Bottom hint
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 hintPos(cx - 120.0f, vpPos.y + vpSize.y - 35.0f);
    draw->AddText(hintPos, Col32_Text(180),
        "[W/S] NAVIGATE  [ENTER] SELECT");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
