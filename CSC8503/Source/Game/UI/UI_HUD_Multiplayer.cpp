/**
 * @file UI_HUD_Multiplayer.cpp
 * @brief HUD sub-module: multiplayer panels (match banner, opponent bar,
 *        disruption effect, network status).
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

/// @brief Render match banner ("WAITING FOR OPPONENT" or "MATCH START").
///
/// Banner timer is stored in Res_UIState::matchBannerTimer to avoid static state.
/// Fades out over 2.2 seconds after match start.
void MatchBanner(ImDrawList* draw, const Res_GameState& gs, Res_UIState& ui, float gameW, float globalTime, float dt) {
    if (gs.matchJustStarted) {
        ui.matchBannerTimer = 2.2f;
    } else if (ui.matchBannerTimer > 0.0f) {
        ui.matchBannerTimer = std::max(0.0f, ui.matchBannerTimer - dt);
    }

    const bool showWaiting = gs.matchPhase == MatchPhase::WaitingForPeer;
    const bool showStart = gs.matchPhase == MatchPhase::Running && ui.matchBannerTimer > 0.0f;
    if (!showWaiting && !showStart) return;

    const char* title = showWaiting ? "WAITING FOR OPPONENT" : "MATCH START";
    const char* subtitle = showWaiting ? "STANDING BY FOR CONNECTION" : "MOVE OUT";

    ImFont* titleFont = GetFont_Terminal();
    ImFont* smallFont = GetFont_Terminal();

    if (titleFont) ImGui::PushFont(titleFont);
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    if (titleFont) ImGui::PopFont();

    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 subtitleSize = ImGui::CalcTextSize(subtitle);
    if (smallFont) ImGui::PopFont();

    const float padX = 30.0f;
    const float padTop = 16.0f;
    const float padBottom = 14.0f;
    const float boxW = std::max(titleSize.x, subtitleSize.x) + padX * 2.0f;
    const float boxH = titleSize.y + subtitleSize.y + padTop + padBottom + 12.0f;
    const float boxX = gameW * 0.5f - boxW * 0.5f;
    const float boxY = gs.countdownActive ? 136.0f : 96.0f;

    float fade = 1.0f;
    if (showStart) {
        fade = std::min(1.0f, ui.matchBannerTimer / 0.35f);
        fade = std::min(fade, ui.matchBannerTimer / 2.2f + 0.25f);
    }

    const float pulse = showWaiting ? (sinf(globalTime * 2.8f) + 1.0f) * 0.5f : 1.0f;
    const ImU32 bgCol = Col32_BgDark(static_cast<uint8_t>(160.0f * fade));
    const ImU32 borderCol = showWaiting
        ? Col32_Gray(static_cast<uint8_t>((120.0f + pulse * 60.0f) * fade))
        : Col32_Accent(static_cast<uint8_t>(190.0f * fade));
    const ImU32 accentCol = Col32_Accent(static_cast<uint8_t>((showWaiting ? (205.0f + pulse * 30.0f) : 235.0f) * fade));
    const ImU32 subtitleCol = Col32_Bg(static_cast<uint8_t>(190.0f * fade));

    draw->AddRectFilled(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), bgCol, 5.0f);
    draw->AddRect(ImVec2(boxX, boxY), ImVec2(boxX + boxW, boxY + boxH), borderCol, 5.0f, 0, 1.4f);
    draw->AddLine(ImVec2(boxX + 18.0f, boxY + 38.0f), ImVec2(boxX + boxW - 18.0f, boxY + 38.0f),
        Col32_Gray(static_cast<uint8_t>(82.0f * fade)), 1.0f);

    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(gameW * 0.5f - titleSize.x * 0.5f, boxY + padTop), accentCol, title);
    if (titleFont) ImGui::PopFont();

    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(gameW * 0.5f - subtitleSize.x * 0.5f, boxY + padTop + titleSize.y + 10.0f),
        subtitleCol, subtitle);
    if (smallFont) ImGui::PopFont();
}

/// @brief Render opponent progress bar (3-segment colored bar).
void OpponentBar(ImDrawList* draw, const Res_GameState& gs, float gameW) {
    if (gs.matchPhase == MatchPhase::WaitingForPeer || gs.matchPhase == MatchPhase::Starting) {
        return;
    }

    ImFont* titleFont = GetFont_Terminal();

    constexpr float barW = 340.0f;
    constexpr float barH = 14.0f;
    constexpr float segmentGap = 4.0f;
    float barX = gameW * 0.5f - barW * 0.5f;
    float barY = gs.countdownActive ? 78.0f : 56.0f;

    const uint8_t opponentStage = std::min<uint8_t>(gs.opponentStageProgress, kMultiplayerStageCount);
    const float segmentW = (barW - segmentGap * (kMultiplayerStageCount - 1)) / static_cast<float>(kMultiplayerStageCount);

    // Game mechanic colors
    const ImU32 inactiveCol = IM_COL32(125, 125, 125, 105);
    const ImU32 stageCols[kMultiplayerStageCount] = {
        IM_COL32(118, 154, 109, 225),
        IM_COL32(252, 111, 41, 225),
        IM_COL32(220, 60, 40, 225),
    };

    // Title
    if (titleFont) ImGui::PushFont(titleFont);
    const char* title = "OPPONENT PROGRESS";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    draw->AddText(ImVec2(gameW * 0.5f - titleSize.x * 0.5f, barY - 24.0f),
        IM_COL32(220, 60, 40, 220), title);
    if (titleFont) ImGui::PopFont();

    // Segmented bar
    for (uint8_t i = 0; i < kMultiplayerStageCount; ++i) {
        const float segX = barX + i * (segmentW + segmentGap);
        const ImVec2 oppMin(segX, barY);
        const ImVec2 oppMax(segX + segmentW, barY + barH);
        const ImU32 oppCol = (i < opponentStage) ? stageCols[i] : inactiveCol;

        draw->AddRectFilled(oppMin, oppMax, oppCol, 2.0f);
        draw->AddRect(oppMin, oppMax, Col32_Gray(95), 2.0f, 0, 1.0f);
    }
}

/// @brief Render full-screen disruption visual effect (type 1=vision, 2=speed, 3=signal).
void DisruptionEffect(ImDrawList* draw, const Res_GameState& gs,
                       float displayW, float displayH, float globalTime) {
    if (gs.disruptionType == 0 || gs.disruptionTimer <= 0.0f) return;

    float progress = (gs.disruptionDuration > 0.001f)
        ? std::clamp(gs.disruptionTimer / gs.disruptionDuration, 0.0f, 1.0f)
        : 0.0f;

    // Disruption effect colors — game mechanic, kept as literals
    switch (gs.disruptionType) {
        case 1: {
            float pulse = (sinf(globalTime * 8.0f) + 1.0f) * 0.5f;
            uint8_t alpha = (uint8_t)(40.0f + pulse * 60.0f * progress);
            draw->AddRectFilledMultiColor(
                ImVec2(0, 0), ImVec2(displayW, 40.0f),
                IM_COL32(220, 40, 40, alpha), IM_COL32(220, 40, 40, alpha),
                IM_COL32(220, 40, 40, 0), IM_COL32(220, 40, 40, 0));
            draw->AddRectFilledMultiColor(
                ImVec2(0, displayH - 40.0f), ImVec2(displayW, displayH),
                IM_COL32(220, 40, 40, 0), IM_COL32(220, 40, 40, 0),
                IM_COL32(220, 40, 40, alpha), IM_COL32(220, 40, 40, alpha));
            break;
        }
        case 2: {
            uint8_t alpha = (uint8_t)(20.0f * progress);
            float spacing = 30.0f;
            for (float x = 0; x < displayW; x += spacing) {
                draw->AddLine(ImVec2(x, 0), ImVec2(x, displayH),
                    IM_COL32(60, 100, 220, alpha), 1.0f);
            }
            for (float y = 0; y < displayH; y += spacing) {
                draw->AddLine(ImVec2(0, y), ImVec2(displayW, y),
                    IM_COL32(60, 100, 220, alpha), 1.0f);
            }
            break;
        }
        case 3: {
            unsigned int seed = (unsigned int)(globalTime * 2000.0f);
            int blockCount = (int)(15.0f * progress);
            for (int i = 0; i < blockCount; ++i) {
                seed = seed * 1103515245u + 12345u;
                float bx = (float)(seed % (unsigned int)displayW);
                seed = seed * 1103515245u + 12345u;
                float by = (float)(seed % (unsigned int)displayH);
                seed = seed * 1103515245u + 12345u;
                float bw = 20.0f + (float)(seed % 80);
                seed = seed * 1103515245u + 12345u;
                float bh = 2.0f + (float)(seed % 8);
                uint8_t a = (uint8_t)(30 + (seed % 40));
                draw->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh),
                    Col32_Accent(a));
            }
            break;
        }
        default: break;
    }

    // "DISRUPTED" indicator
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* labels[] = { "", "VISION IMPAIRED", "SPEED REDUCED", "SIGNAL SCRAMBLED" };
    const char* label = (gs.disruptionType < 4) ? labels[gs.disruptionType] : "";
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    float labelX = displayW * 0.5f - labelSize.x * 0.5f;

    float blink = (sinf(globalTime * 6.0f) + 1.0f) * 0.5f;
    uint8_t lblAlpha = (uint8_t)(150 + blink * 105);
    draw->AddText(ImVec2(labelX, 70.0f), IM_COL32(220, 40, 40, lblAlpha), label);

    if (termFont) ImGui::PopFont();
}

/// @brief Render bottom-right network ping display (green/yellow/red by latency).
void NetworkStatus(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH) {
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    char pingBuf[24];
    snprintf(pingBuf, sizeof(pingBuf), "PING: %ums", gs.networkPing);

    // Color based on ping quality (game mechanic)
    ImU32 pingCol;
    if (gs.networkPing < 50)       pingCol = IM_COL32(80, 200, 120, 180);
    else if (gs.networkPing < 100) pingCol = IM_COL32(220, 200, 0, 180);
    else                           pingCol = IM_COL32(220, 60, 40, 180);

    ImVec2 pingSize = ImGui::CalcTextSize(pingBuf);
    draw->AddText(
        ImVec2(gameW - pingSize.x - 20.0f, displayH - 78.0f),
        pingCol, pingBuf);

    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
