/**
 * @file UI_GhostDuelResult.cpp
 * @brief Ghost Duel result screen: dual-column score breakdown + victory/defeat/draw.
 */
#include "UI_GhostDuelResult.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_GhostDuelState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Input.h"
#include "Keyboard.h"
#include "Mouse.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// Helper: draw one column of score breakdown
// ============================================================
/**
 * @brief 绘制单侧积分扣分明细列（左=YOU / 右=对手）。
 * @param draw       ImGui draw-list
 * @param colX       列起始 X 坐标
 * @param startY     列起始 Y 坐标
 * @param score      最终积分
 * @param lost_time  时间衰减扣分
 * @param lost_kills 击杀扣分
 * @param lost_items 道具使用扣分
 * @param lost_countdown 倒计时扣分
 * @param lost_failure   失败扣分
 * @param killCount  击杀次数
 * @param itemUseCount 道具使用次数
 * @param playerLabel 列标题文本
 * @param isWinner   是否为胜者（Teal 下划线标注）
 */
static void DrawScoreColumn(ImDrawList* draw, float colX, float startY,
                             int32_t score, int32_t lost_time, int32_t lost_kills,
                             int32_t lost_items, int32_t lost_countdown, int32_t lost_failure,
                             int16_t killCount, int16_t itemUseCount,
                             const char* playerLabel, bool isWinner)
{
    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImU32 labelCol  = IM_COL32(16, 13, 10, 220);
    ImU32 deductCol = IM_COL32(220, 60, 40, 220);
    ImU32 zeroCol   = IM_COL32(120, 120, 120, 180);
    char buf[64];

    float valX = colX + 140.0f;
    float rowH = 24.0f;
    float y = startY;

    if (termFont) ImGui::PushFont(termFont);

    // Player label (with teal underline if winner)
    ImU32 nameCol = isWinner ? IM_COL32(46, 196, 182, 255) : IM_COL32(16, 13, 10, 220);
    draw->AddText(ImVec2(colX, y), nameCol, playerLabel);
    if (isWinner) {
        ImVec2 nameSize = ImGui::CalcTextSize(playerLabel);
        draw->AddLine(ImVec2(colX, y + nameSize.y + 2.0f),
                      ImVec2(colX + nameSize.x, y + nameSize.y + 2.0f),
                      IM_COL32(46, 196, 182, 150), 2.0f);
    }
    y += rowH + 4.0f;

    // Separator
    draw->AddLine(ImVec2(colX, y), ImVec2(valX + 60.0f, y),
        IM_COL32(200, 200, 200, 100), 1.0f);
    y += 8.0f;

    // INITIAL
    draw->AddText(ImVec2(colX, y), labelCol, "INITIAL:");
    draw->AddText(ImVec2(valX, y), IM_COL32(80, 200, 120, 220), "1000");
    y += rowH;

    // TIME
    snprintf(buf, sizeof(buf), "TIME (-1/s):");
    draw->AddText(ImVec2(colX, y), labelCol, buf);
    if (lost_time > 0) {
        snprintf(buf, sizeof(buf), "-%d", lost_time);
        draw->AddText(ImVec2(valX, y), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, y), zeroCol, "0");
    }
    y += rowH;

    // KILLS
    snprintf(buf, sizeof(buf), "KILLS (x%d):", killCount);
    draw->AddText(ImVec2(colX, y), labelCol, buf);
    if (lost_kills > 0) {
        snprintf(buf, sizeof(buf), "-%d", lost_kills);
        draw->AddText(ImVec2(valX, y), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, y), zeroCol, "0");
    }
    y += rowH;

    // ITEMS
    snprintf(buf, sizeof(buf), "ITEMS (x%d):", itemUseCount);
    draw->AddText(ImVec2(colX, y), labelCol, buf);
    if (lost_items > 0) {
        snprintf(buf, sizeof(buf), "-%d", lost_items);
        draw->AddText(ImVec2(valX, y), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, y), zeroCol, "0");
    }
    y += rowH;

    // COUNTDOWN
    draw->AddText(ImVec2(colX, y), labelCol, "COUNTDOWN:");
    if (lost_countdown > 0) {
        snprintf(buf, sizeof(buf), "-%d", lost_countdown);
        draw->AddText(ImVec2(valX, y), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, y), zeroCol, "0");
    }
    y += rowH;

    // FAILURE
    draw->AddText(ImVec2(colX, y), labelCol, "FAILURE:");
    if (lost_failure > 0) {
        snprintf(buf, sizeof(buf), "-%d", lost_failure);
        draw->AddText(ImVec2(valX, y), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, y), zeroCol, "0");
    }
    y += rowH + 4.0f;

    // Separator
    draw->AddLine(ImVec2(colX, y), ImVec2(valX + 60.0f, y),
        IM_COL32(200, 200, 200, 100), 1.0f);
    y += 8.0f;

    // FINAL SCORE
    const char* rating = GetScoreRating(score);
    int8_t tier = GetScoreRatingTier(score);
    ImU32 ratingCol = UITheme::GetScoreRatingColor(tier);

    snprintf(buf, sizeof(buf), "FINAL: %d [%s]", std::max(0, score), rating);
    draw->AddText(ImVec2(colX, y), ratingCol, buf);

    if (termFont) ImGui::PopFont();
}

// ============================================================
// RenderGhostDuelResultScreen
// ============================================================

static const char* kResultItems[] = { "RETRY", "RETURN TO MENU" };
static constexpr int kResultItemCount = 2;

/**
 * @brief 渲染幽影对决结算画面：双列扣分明细对比 + 胜负判定 + RETRY/RETURN 菜单。
 * @details 读取 Res_UIState（本地积分）与 Res_GhostDuelState（对手积分）比较胜负，
 *          菜单选择写回 Res_UIState::pendingSceneRequest。
 * @param registry 当前 ECS 注册表
 * @param dt       帧时间步长（未使用）
 */
void RenderGhostDuelResultScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    const auto& input = registry.ctx<Res_Input>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##GhostDuelResult", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    float cx = vpPos.x + vpSize.x * 0.5f;

    // Read ghost duel state
    int32_t oppScore = 0, oppLost_time = 0, oppLost_kills = 0, oppLost_items = 0;
    int32_t oppLost_countdown = 0, oppLost_failure = 0;
    int16_t oppKillCount = 0, oppItemUseCount = 0;
    char oppName[16] = "RIVAL";
    bool opponentFinished = false;
    bool isDisconnected = false;

    if (registry.has_ctx<Res_GhostDuelState>()) {
        const auto& gd = registry.ctx<Res_GhostDuelState>();
        oppScore         = gd.opponentScore;
        oppLost_time     = gd.opponentScoreLost_time;
        oppLost_kills    = gd.opponentScoreLost_kills;
        oppLost_items    = gd.opponentScoreLost_items;
        oppLost_countdown = gd.opponentScoreLost_countdown;
        oppLost_failure  = gd.opponentScoreLost_failure;
        oppKillCount     = gd.opponentKillCount;
        oppItemUseCount  = gd.opponentItemUseCount;
        strncpy_s(oppName, sizeof(oppName), gd.opponentName, _TRUNCATE);
        opponentFinished = gd.opponentFinished;
    }

    if (registry.has_ctx<Res_GameState>()) {
        const auto& gs = registry.ctx<Res_GameState>();
        isDisconnected = (gs.matchResult == MatchResult::Disconnected);
    }

    const int32_t localScore = ui.campaignScore;

    // ── Title ─────────────────────────────────────────────────
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* mainTitle = "GHOST DUEL COMPLETE";
    ImVec2 titleSize = ImGui::CalcTextSize(mainTitle);
    float titleY = vpPos.y + vpSize.y * 0.06f;
    draw->AddText(ImVec2(cx - titleSize.x * 0.5f, titleY),
        IM_COL32(46, 196, 182, 255), mainTitle);

    if (titleFont) ImGui::PopFont();

    // ── Victory / Defeat / Draw ──────────────────────────────
    const char* resultText;
    ImU32 resultColor;
    if (isDisconnected) {
        resultText = "PEER DISCONNECTED";
        resultColor = IM_COL32(120, 120, 120, 255);
    } else if (localScore > oppScore) {
        resultText = "VICTORY";
        resultColor = IM_COL32(252, 111, 41, 255);
    } else if (localScore < oppScore) {
        resultText = "DEFEAT";
        resultColor = IM_COL32(220, 60, 40, 255);
    } else {
        resultText = "DRAW";
        resultColor = IM_COL32(180, 140, 40, 255);
    }

    bool localIsWinner = (localScore >= oppScore && !isDisconnected);
    bool oppIsWinner   = (oppScore >= localScore && !isDisconnected);

    // ── Dual column score breakdown ───────────────────────────
    float colSpacing = 80.0f;
    float colWidth   = 220.0f;
    float leftColX   = cx - colSpacing * 0.5f - colWidth;
    float rightColX  = cx + colSpacing * 0.5f;
    float columnsY   = titleY + titleSize.y + 20.0f;

    DrawScoreColumn(draw, leftColX, columnsY,
        localScore, ui.scoreLost_time, ui.scoreLost_kills,
        ui.scoreLost_items, ui.scoreLost_countdown, ui.scoreLost_failure,
        ui.scoreKillCount, ui.scoreItemUseCount,
        "-- YOU --", localIsWinner && !oppIsWinner);

    DrawScoreColumn(draw, rightColX, columnsY,
        oppScore, oppLost_time, oppLost_kills,
        oppLost_items, oppLost_countdown, oppLost_failure,
        oppKillCount, oppItemUseCount,
        oppName, oppIsWinner && !localIsWinner);

    // ── Central result banner ─────────────────────────────────
    float bannerY = columnsY + 230.0f;

    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 resultSize = ImGui::CalcTextSize(resultText);
    draw->AddText(ImVec2(cx - resultSize.x * 0.5f, bannerY), resultColor, resultText);

    // Decorative lines around result
    float lineW = resultSize.x * 0.4f;
    float lineGap = 12.0f;
    float lineYc = bannerY + resultSize.y * 0.5f;
    draw->AddLine(ImVec2(cx - resultSize.x * 0.5f - lineGap - lineW, lineYc),
                  ImVec2(cx - resultSize.x * 0.5f - lineGap, lineYc),
                  resultColor, 2.0f);
    draw->AddLine(ImVec2(cx + resultSize.x * 0.5f + lineGap, lineYc),
                  ImVec2(cx + resultSize.x * 0.5f + lineGap + lineW, lineYc),
                  resultColor, 2.0f);
    if (titleFont) ImGui::PopFont();

    // ── Menu items ────────────────────────────────────────────
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = bannerY + 50.0f;
    float menuItemH  = 38.0f;

    // Navigation
    if (input.keyPressed[KeyCodes::W] || input.keyPressed[KeyCodes::UP]) {
        ui.ghostDuelResultSelectedIndex =
            (ui.ghostDuelResultSelectedIndex - 1 + kResultItemCount) % kResultItemCount;
    }
    if (input.keyPressed[KeyCodes::S] || input.keyPressed[KeyCodes::DOWN]) {
        ui.ghostDuelResultSelectedIndex =
            (ui.ghostDuelResultSelectedIndex + 1) % kResultItemCount;
    }

    float itemW = 240.0f;
    float itemStartX = cx - itemW * 0.5f;

    // Mouse hover
    for (int i = 0; i < kResultItemCount; ++i) {
        ImVec2 itemMin(itemStartX, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(itemStartX + itemW, menuStartY + i * menuItemH + menuItemH - 6.0f);
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
            mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
            ui.ghostDuelResultSelectedIndex = static_cast<int8_t>(i);
        }
    }

    // Confirm
    int8_t confirmedIndex = -1;
    if (input.keyPressed[KeyCodes::RETURN] || input.keyPressed[KeyCodes::SPACE]) {
        confirmedIndex = ui.ghostDuelResultSelectedIndex;
    }
    if (input.mouseButtonPressed[NCL::MouseButtons::Left]) {
        ImVec2 mousePos = ImGui::GetMousePos();
        for (int i = 0; i < kResultItemCount; ++i) {
            ImVec2 itemMin(itemStartX, menuStartY + i * menuItemH - 2.0f);
            ImVec2 itemMax(itemStartX + itemW, menuStartY + i * menuItemH + menuItemH - 6.0f);
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                confirmedIndex = static_cast<int8_t>(i);
                break;
            }
        }
    }

    // Render menu items
    for (int i = 0; i < kResultItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.ghostDuelResultSelectedIndex);
        bool isDisabled = (i == 0 && isDisconnected);  // RETRY disabled when disconnected

        ImVec2 itemMin(itemStartX, itemY - 2.0f);
        ImVec2 itemMax(itemStartX + itemW, itemY + menuItemH - 6.0f);

        if (isSelected && !isDisabled) {
            draw->AddRectFilled(itemMin, itemMax,
                IM_COL32(46, 196, 182, 25), 2.0f);
            draw->AddRect(itemMin, itemMax,
                IM_COL32(46, 196, 182, 120), 2.0f, 0, 1.0f);
        }

        char label[64];
        snprintf(label, sizeof(label), isSelected ? "> %s" : "  %s", kResultItems[i]);
        ImU32 textColor;
        if (isDisabled) {
            textColor = IM_COL32(120, 120, 120, 150);
        } else if (isSelected) {
            textColor = IM_COL32(16, 13, 10, 255);
        } else {
            textColor = IM_COL32(16, 13, 10, 220);
        }
        float textX = itemStartX + 10.0f + (isSelected ? 4.0f : 0.0f);
        draw->AddText(ImVec2(textX, itemY), textColor, label);
    }

    if (termFont) ImGui::PopFont();

    // Confirm action
    if (confirmedIndex >= 0) {
        switch (confirmedIndex) {
            case 0: // RETRY
                if (!isDisconnected) {
                    if (registry.has_ctx<Res_GameState>()
                        && registry.ctx<Res_GameState>().isMultiplayer
                        && registry.ctx<Res_GameState>().matchResult != MatchResult::Disconnected) {
                        ui.multiplayerRetryRequested = true;
                    } else {
                        ui.pendingSceneRequest = SceneRequest::RestartLevel;
                    }
                    LOG_INFO("[UI_GhostDuelResult] RETRY");
                }
                break;
            case 1: // RETURN TO MENU
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[UI_GhostDuelResult] ReturnToMenu");
                break;
            default: break;
        }
    }

    // Bottom hint
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 hintPos(cx - 120.0f, vpPos.y + vpSize.y - 35.0f);
    draw->AddText(hintPos, IM_COL32(16, 13, 10, 180),
        "[W/S] NAVIGATE  [ENTER] SELECT");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
