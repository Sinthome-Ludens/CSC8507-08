/**
 * @file UI_Victory.cpp
 * @brief 战役通关画面 UI（全屏）。
 */
#include "UI_Victory.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_Input.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"
#include "Keyboard.h"
#include "Mouse.h"

using namespace NCL;

namespace ECS::UI {

/**
 * @brief 渲染全屏 Victory 画面。
 *
 * @details
 * 读取 `Res_UIState` 获取 `totalPlayTime` 和 `mapSequence` 已通关地图名，
 * 渲染时间统计和地图列表。玩家点击 "RETURN TO MENU" 或按 Enter/Space 时
 * 设置 `SceneRequest::ReturnToMenu` 触发场景切换。
 */
void RenderVictoryScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    const auto& input = registry.ctx<Res_Input>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##Victory", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    float cx = vpPos.x + vpSize.x * 0.5f;

    // ── Title ─────────────────────────────────────────────────
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    const char* title = "CAMPAIGN COMPLETE";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = cx - titleSize.x * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.13f;
    draw->AddText(ImVec2(titleX, titleY),
        IM_COL32(252, 111, 41, 255), title);
    if (titleFont) ImGui::PopFont();

    // ── Subtitle ──────────────────────────────────────────────
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);
    const char* subtitle = "ALL AREAS SECURED";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    draw->AddText(ImVec2(cx - subSize.x * 0.5f, titleY + titleSize.y + 6.0f),
        IM_COL32(16, 13, 10, 180), subtitle);
    if (termFont) ImGui::PopFont();

    // ── Decorative line ───────────────────────────────────────
    float lineY = titleY + titleSize.y + 36.0f;
    draw->AddLine(ImVec2(cx - 140.0f, lineY), ImVec2(cx + 140.0f, lineY),
        IM_COL32(200, 200, 200, 120), 1.0f);

    // ── Statistics ────────────────────────────────────────────
    if (termFont) ImGui::PushFont(termFont);

    float statsY = lineY + 20.0f;
    float statsX = cx - 130.0f;

    // Total play time (accumulated in Res_UIState by Sys_LevelGoal)
    float totalTime = ui.totalPlayTime;
    int totalSec = static_cast<int>(totalTime);
    int mm = totalSec / 60;
    int ss = totalSec % 60;
    char buf[64];

    snprintf(buf, sizeof(buf), "TOTAL TIME:  %02d:%02d", mm, ss);
    draw->AddText(ImVec2(statsX, statsY),
        IM_COL32(16, 13, 10, 220), buf);

    // ── Map names ─────────────────────────────────────────────
    float mapsY = statsY + 40.0f;
    draw->AddText(ImVec2(statsX, mapsY),
        IM_COL32(16, 13, 10, 220), "CLEARED MAPS:");

    for (int i = 0; i < Res_UIState::MAP_SEQUENCE_LENGTH; ++i) {
        int idx = ui.mapSequence[i];
        const char* mapName = (idx >= 0 && idx < kMapCount)
            ? kMapDisplayNames[idx] : "???";
        snprintf(buf, sizeof(buf), "  %d. %s", i + 1, mapName);
        draw->AddText(ImVec2(statsX + 10.0f, mapsY + 26.0f + i * 24.0f),
            IM_COL32(252, 111, 41, 220), buf);
    }

    // ── Score + Rating ───────────────────────────────────────
    float scoreY = mapsY + 26.0f + Res_UIState::MAP_SEQUENCE_LENGTH * 24.0f + 16.0f;
    int32_t finalScore = ui.campaignScore;
    const char* rating = GetScoreRating(finalScore);
    int8_t ratingTier = GetScoreRatingTier(finalScore);

    const ImU32 ratingCol = UITheme::GetScoreRatingColor(ratingTier);

    ImU32 labelCol  = IM_COL32(16, 13, 10, 220);
    ImU32 deductCol = IM_COL32(220, 60, 40, 220);
    float valX = statsX + 180.0f;

    snprintf(buf, sizeof(buf), "FINAL SCORE:  %d  [%s]", std::max(0, finalScore), rating);
    draw->AddText(ImVec2(statsX, scoreY), ratingCol, buf);

    // Breakdown
    draw->AddText(ImVec2(statsX, scoreY + 28.0f), labelCol, "-- BREAKDOWN --");

    snprintf(buf, sizeof(buf), "TIME (-1/s):");
    draw->AddText(ImVec2(statsX + 10.0f, scoreY + 52.0f), labelCol, buf);
    if (ui.scoreLost_time > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_time);
        draw->AddText(ImVec2(valX, scoreY + 52.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, scoreY + 52.0f), labelCol, "0");
    }

    snprintf(buf, sizeof(buf), "KILLS (x%d):", ui.scoreKillCount);
    draw->AddText(ImVec2(statsX + 10.0f, scoreY + 76.0f), labelCol, buf);
    if (ui.scoreLost_kills > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_kills);
        draw->AddText(ImVec2(valX, scoreY + 76.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, scoreY + 76.0f), labelCol, "0");
    }

    snprintf(buf, sizeof(buf), "ITEMS (x%d):", ui.scoreItemUseCount);
    draw->AddText(ImVec2(statsX + 10.0f, scoreY + 100.0f), labelCol, buf);
    if (ui.scoreLost_items > 0) {
        snprintf(buf, sizeof(buf), "-%d", ui.scoreLost_items);
        draw->AddText(ImVec2(valX, scoreY + 100.0f), deductCol, buf);
    } else {
        draw->AddText(ImVec2(valX, scoreY + 100.0f), labelCol, "0");
    }

    if (termFont) ImGui::PopFont();

    // ── Separator ─────────────────────────────────────────────
    float sepY = scoreY + 130.0f;
    draw->AddLine(ImVec2(cx - 80.0f, sepY), ImVec2(cx + 80.0f, sepY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // ── Menu: RETURN TO MENU ──────────────────────────────────
    if (termFont) ImGui::PushFont(termFont);

    float menuY = sepY + 20.0f;
    float itemW = 240.0f;
    float itemX = cx - itemW * 0.5f;
    ImVec2 itemMin(itemX, menuY - 2.0f);
    ImVec2 itemMax(itemX + itemW, menuY + 32.0f);

    // Always highlighted (single option)
    bool hovered = false;
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        hovered = (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                   mousePos.y >= itemMin.y && mousePos.y <= itemMax.y);
    }

    draw->AddRectFilled(itemMin, itemMax,
        IM_COL32(252, 111, 41, 25), 2.0f);
    draw->AddRect(itemMin, itemMax,
        IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);

    draw->AddText(ImVec2(itemX + 14.0f, menuY),
        IM_COL32(16, 13, 10, 255), "> RETURN TO MENU");

    if (termFont) ImGui::PopFont();

    // ── Confirm action ────────────────────────────────────────
    bool confirmed = false;
    if (input.keyPressed[KeyCodes::RETURN] || input.keyPressed[KeyCodes::SPACE]) {
        confirmed = true;
    }
    if (input.mouseButtonPressed[NCL::MouseButtons::Left] && hovered) {
        confirmed = true;
    }

    if (confirmed) {
        ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
        LOG_INFO("[UI_Victory] Victory -> ReturnToMenu");
    }

    // ── Bottom hint ───────────────────────────────────────────
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(cx - 60.0f, vpPos.y + vpSize.y - 35.0f),
        IM_COL32(16, 13, 10, 180), "[ENTER] SELECT");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
