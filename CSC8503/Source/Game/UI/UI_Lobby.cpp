/**
 * @file UI_Lobby.cpp
 * @brief 多人大厅 UI。
 */
#include "UI_Lobby.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_UIKeyConfig.h"

using namespace NCL;
using namespace ECS::UITheme;

namespace ECS::UI {

// ============================================================
// Static data
// ============================================================

static const char* kLobbyItems[] = {
    "HOST GAME",
    "JOIN GAME",
    "BACK",
};
static constexpr int kLobbyItemCount = 3;

// ============================================================
// RenderLobbyScreen
// ============================================================

void RenderLobbyScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    if (!registry.has_ctx<Res_LobbyState>()) return;
    auto& lobby = registry.ctx<Res_LobbyState>();

    const auto& input = registry.ctx<Res_Input>();

    Res_UIKeyConfig defaultUiCfg;
    const auto& uiCfg = registry.has_ctx<Res_UIKeyConfig>() ? registry.ctx<Res_UIKeyConfig>() : defaultUiCfg;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##LobbyScreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg());

    float cx = vpPos.x + vpSize.x * 0.5f;

    // ── Title ─────────────────────────────────────────────────
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "MULTIPLAYER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = cx - titleSize.x * 0.5f;
    float titleY = vpPos.y + 60.0f;
    draw->AddText(ImVec2(titleX, titleY), Col32_Text(), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    const char* subtitle = "COMPETITIVE 1v1 OPERATIONS";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    float subY = titleY + titleSize.y + 6.0f;
    draw->AddText(ImVec2(cx - subSize.x * 0.5f, subY),
        Col32_Text(180), subtitle);

    if (smallFont) ImGui::PopFont();

    // Separator
    float lineY = subY + 26.0f;
    draw->AddLine(
        ImVec2(cx - 160.0f, lineY), ImVec2(cx + 160.0f, lineY),
        Col32_Gray(120), 1.0f);

    // ── Mode cards ────────────────────────────────────────────
    float cardW = 280.0f;
    float cardH = 140.0f;
    float cardGap = 40.0f;
    float cardsStartY = lineY + 30.0f;
    float hostCardX = cx - cardW - cardGap * 0.5f;
    float joinCardX = cx + cardGap * 0.5f;

    ImFont* termFont = GetFont_Terminal();

    // Keyboard navigation
    if (!lobby.ipInputActive) {
        if (input.keyPressed[uiCfg.keyMenuUp] || input.keyPressed[uiCfg.keyMenuUpAlt] ||
            input.keyPressed[uiCfg.keyMenuLeft] || input.keyPressed[uiCfg.keyMenuLeftAlt]) {
            ui.lobbySelectedIndex = (ui.lobbySelectedIndex - 1 + kLobbyItemCount) % kLobbyItemCount;
        }
        if (input.keyPressed[uiCfg.keyMenuDown] || input.keyPressed[uiCfg.keyMenuDownAlt] ||
            input.keyPressed[uiCfg.keyMenuRight] || input.keyPressed[uiCfg.keyMenuRightAlt]) {
            ui.lobbySelectedIndex = (ui.lobbySelectedIndex + 1) % kLobbyItemCount;
        }
    }

    // Mouse hover detection for cards
    ImVec2 mousePos = ImGui::GetMousePos();

    // Card 0: HOST
    ImVec2 hostMin(hostCardX, cardsStartY);
    ImVec2 hostMax(hostCardX + cardW, cardsStartY + cardH);
    if (mousePos.x >= hostMin.x && mousePos.x <= hostMax.x &&
        mousePos.y >= hostMin.y && mousePos.y <= hostMax.y) {
        ui.lobbySelectedIndex = 0;
    }

    // Card 1: JOIN
    ImVec2 joinMin(joinCardX, cardsStartY);
    ImVec2 joinMax(joinCardX + cardW, cardsStartY + cardH);
    if (mousePos.x >= joinMin.x && mousePos.x <= joinMax.x &&
        mousePos.y >= joinMin.y && mousePos.y <= joinMax.y) {
        ui.lobbySelectedIndex = 1;
    }

    // Draw HOST card
    {
        bool sel = (ui.lobbySelectedIndex == 0);
        ImU32 bgCol = sel ? Col32_Accent(20) : Col32_Bg();
        ImU32 bdCol = sel ? Col32_Accent(150) : Col32_Gray(100);

        draw->AddRectFilled(hostMin, hostMax, bgCol, 4.0f);
        draw->AddRect(hostMin, hostMax, bdCol, 4.0f, 0, sel ? 2.0f : 1.0f);

        if (titleFont) ImGui::PushFont(titleFont);
        const char* hostTitle = "HOST";
        ImVec2 htSize = ImGui::CalcTextSize(hostTitle);
        draw->AddText(
            ImVec2(hostCardX + (cardW - htSize.x) * 0.5f, cardsStartY + 20.0f),
            Col32_Text(), hostTitle);
        if (titleFont) ImGui::PopFont();

        if (smallFont) ImGui::PushFont(smallFont);
        const char* hostDesc = "Create a server and wait\nfor an opponent to connect";
        draw->AddText(
            ImVec2(hostCardX + 20.0f, cardsStartY + 65.0f),
            Col32_Text(160), hostDesc);

        char portBuf[32];
        snprintf(portBuf, sizeof(portBuf), "PORT: %u", lobby.port);
        draw->AddText(
            ImVec2(hostCardX + 20.0f, cardsStartY + cardH - 28.0f),
            Col32_Accent(200), portBuf);
        if (smallFont) ImGui::PopFont();
    }

    // Draw JOIN card
    {
        bool sel = (ui.lobbySelectedIndex == 1);
        ImU32 bgCol = sel ? Col32_Accent(20) : Col32_Bg();
        ImU32 bdCol = sel ? Col32_Accent(150) : Col32_Gray(100);

        draw->AddRectFilled(joinMin, joinMax, bgCol, 4.0f);
        draw->AddRect(joinMin, joinMax, bdCol, 4.0f, 0, sel ? 2.0f : 1.0f);

        if (titleFont) ImGui::PushFont(titleFont);
        const char* joinTitle = "JOIN";
        ImVec2 jtSize = ImGui::CalcTextSize(joinTitle);
        draw->AddText(
            ImVec2(joinCardX + (cardW - jtSize.x) * 0.5f, cardsStartY + 20.0f),
            Col32_Text(), joinTitle);
        if (titleFont) ImGui::PopFont();

        if (smallFont) ImGui::PushFont(smallFont);
        const char* joinDesc = "Connect to a host's server\nand start competitive match";
        draw->AddText(
            ImVec2(joinCardX + 20.0f, cardsStartY + 65.0f),
            Col32_Text(160), joinDesc);
        if (smallFont) ImGui::PopFont();
    }

    // ── IP Input (below JOIN card) ────────────────────────────
    float ipY = cardsStartY + cardH + 20.0f;

    if (termFont) ImGui::PushFont(termFont);

    draw->AddText(ImVec2(joinCardX, ipY),
        Col32_Text(200), "TARGET IP:");

    // IP input field
    float ipFieldX = joinCardX + 110.0f;
    float ipFieldW = 160.0f;
    float ipFieldH = 24.0f;

    ImVec2 ipMin(ipFieldX, ipY - 2.0f);
    ImVec2 ipMax(ipFieldX + ipFieldW, ipY + ipFieldH);

    bool ipSel = (ui.lobbySelectedIndex == 1);
    draw->AddRectFilled(ipMin, ipMax,
        ipSel ? IM_COL32(255, 255, 255, 200) : Col32_Bg(), 2.0f);
    draw->AddRect(ipMin, ipMax,
        ipSel ? Col32_Accent(180) : Col32_Gray(100), 2.0f);

    draw->AddText(ImVec2(ipFieldX + 6.0f, ipY + 2.0f),
        Col32_Text(), lobby.joinIP);

    // IP editing via ImGui InputText (only when JOIN is selected)
    if (ipSel) {
        ImGui::SetCursorScreenPos(ImVec2(ipFieldX, ipY - 2.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));  // hidden (draw-list text above)
        ImGui::SetNextItemWidth(ipFieldW);
        ImGui::InputText("##IPInput", lobby.joinIP, sizeof(lobby.joinIP),
            ImGuiInputTextFlags_CharsNoBlank);
        lobby.ipInputActive = ImGui::IsItemActive();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    } else {
        lobby.ipInputActive = false;
    }

    if (termFont) ImGui::PopFont();

    // ── BACK button ───────────────────────────────────────────
    float backY = ipY + 50.0f;
    float backW = 140.0f;
    float backH = 32.0f;
    float backX = cx - backW * 0.5f;

    ImVec2 backMin(backX, backY);
    ImVec2 backMax(backX + backW, backY + backH);

    // Mouse hover for BACK
    if (mousePos.x >= backMin.x && mousePos.x <= backMax.x &&
        mousePos.y >= backMin.y && mousePos.y <= backMax.y) {
        ui.lobbySelectedIndex = 2;
    }

    bool backSel = (ui.lobbySelectedIndex == 2);
    draw->AddRectFilled(backMin, backMax,
        backSel ? Col32_Accent(25) : Col32_Bg(), 3.0f);
    draw->AddRect(backMin, backMax,
        backSel ? Col32_Accent(150) : Col32_Gray(100), 3.0f);

    if (termFont) ImGui::PushFont(termFont);
    const char* backText = "< BACK";
    ImVec2 backTextSize = ImGui::CalcTextSize(backText);
    draw->AddText(
        ImVec2(backX + (backW - backTextSize.x) * 0.5f, backY + (backH - backTextSize.y) * 0.5f),
        backSel ? Col32_Text() : Col32_Text(200), backText);
    if (termFont) ImGui::PopFont();

    // ── Confirm action ────────────────────────────────────────
    int8_t confirmedIndex = -1;
    if ((input.keyPressed[uiCfg.keyConfirm] || input.keyPressed[uiCfg.keyConfirmAlt])
        && !lobby.ipInputActive) {
        confirmedIndex = ui.lobbySelectedIndex;
    }
    if (input.mouseButtonPressed[uiCfg.mouseConfirm]) {
        // Check HOST card
        if (mousePos.x >= hostMin.x && mousePos.x <= hostMax.x &&
            mousePos.y >= hostMin.y && mousePos.y <= hostMax.y) {
            confirmedIndex = 0;
        }
        // Check JOIN card
        if (mousePos.x >= joinMin.x && mousePos.x <= joinMax.x &&
            mousePos.y >= joinMin.y && mousePos.y <= joinMax.y) {
            confirmedIndex = 1;
        }
        // Check BACK button
        if (mousePos.x >= backMin.x && mousePos.x <= backMax.x &&
            mousePos.y >= backMin.y && mousePos.y <= backMax.y) {
            confirmedIndex = 2;
        }
    }

    if (confirmedIndex >= 0) {
        switch (confirmedIndex) {
            case 0: // HOST
                ui.pendingSceneRequest = SceneRequest::HostGame;
                LOG_INFO("[UI_Lobby] Lobby -> HostGame (port=" << lobby.port << ")");
                break;
            case 1: // JOIN
                ui.pendingSceneRequest = SceneRequest::JoinGame;
                LOG_INFO("[UI_Lobby] Lobby -> JoinGame (ip=" << lobby.joinIP << ")");
                break;
            case 2: // BACK
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::MainMenu;
                ui.menuSelectedIndex = 0;
                LOG_INFO("[UI_Lobby] Lobby -> MainMenu (BACK)");
                break;
            default: break;
        }
    }

    // ── Bottom hint ───────────────────────────────────────────
    if (smallFont) ImGui::PushFont(smallFont);

    const char* hint = "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] BACK";
    ImVec2 hintSize = ImGui::CalcTextSize(hint);
    draw->AddText(
        ImVec2(cx - hintSize.x * 0.5f, vpPos.y + vpSize.y - 35.0f),
        Col32_Text(180), hint);

    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
