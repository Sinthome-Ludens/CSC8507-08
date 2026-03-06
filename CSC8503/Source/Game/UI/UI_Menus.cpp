#include "UI_Menus.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// Shared navigation helpers
// ============================================================

void NavigateBackFromSettings(Res_UIState& ui) {
    UIScreen backTo = (ui.previousScreen == UIScreen::PauseMenu)
                     ? UIScreen::PauseMenu : UIScreen::MainMenu;
    ui.activeScreen = backTo;
    ui.previousScreen = UIScreen::Settings;
}

// ============================================================
// Static menu data
// ============================================================

static const char* kMenuItems[] = {
    "START OPERATION",
    "MULTIPLAYER",
    "LOADOUT",
    "SETTINGS",
    "TEAM",
    "EXIT",
};
static constexpr int kMenuItemCount = 6;

static const char* kPauseItems[] = {
    "RESUME",
    "SETTINGS",
    "RETURN TO MENU",
};
static constexpr int kPauseItemCount = 3;

// ============================================================
// RenderMenuBackground — Tactical radar animation (right panel)
// ============================================================

static void RenderMenuBackground(float globalTime,
    float panelX, float panelY, float panelW, float panelH)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Grid lines
    constexpr float gridSpacing = 40.0f;
    ImU32 gridColor = IM_COL32(200, 200, 200, 30);

    for (float x = panelX; x < panelX + panelW; x += gridSpacing) {
        draw->AddLine(ImVec2(x, panelY), ImVec2(x, panelY + panelH), gridColor, 1.0f);
    }
    for (float y = panelY; y < panelY + panelH; y += gridSpacing) {
        draw->AddLine(ImVec2(panelX, y), ImVec2(panelX + panelW, y), gridColor, 1.0f);
    }

    // Center radar
    float cx = panelX + panelW * 0.5f;
    float cy = panelY + panelH * 0.5f;
    float maxR = panelH * 0.30f;

    // Radar rings (orange accent)
    draw->AddCircle(ImVec2(cx, cy), maxR,        IM_COL32(252, 111, 41, 30), 64, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), maxR * 0.6f, IM_COL32(252, 111, 41, 25), 48, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), maxR * 0.3f, IM_COL32(252, 111, 41, 20), 32, 1.0f);

    // Crosshairs
    draw->AddLine(ImVec2(cx - maxR, cy), ImVec2(cx + maxR, cy),
        IM_COL32(252, 111, 41, 25), 1.0f);
    draw->AddLine(ImVec2(cx, cy - maxR), ImVec2(cx, cy + maxR),
        IM_COL32(252, 111, 41, 25), 1.0f);

    // Rotating sweep line
    float angle = globalTime * 0.8f;
    float scanX = cx + cosf(angle) * maxR;
    float scanY = cy + sinf(angle) * maxR;
    draw->AddLine(ImVec2(cx, cy), ImVec2(scanX, scanY),
        IM_COL32(252, 111, 41, 100), 2.0f);

    // Sweep trail
    constexpr int trailSegments = 12;
    constexpr float trailAngle  = 0.5f;
    for (int i = 0; i < trailSegments; ++i) {
        float t = (float)i / (float)trailSegments;
        float a = angle - trailAngle * t;
        uint8_t alpha = (uint8_t)(std::min(60.0f * (1.0f - t), 255.0f));
        float r = maxR;
        ImVec2 p1(cx + cosf(a) * r, cy + sinf(a) * r);
        float a2 = angle - trailAngle * (t + 1.0f / trailSegments);
        ImVec2 p2(cx + cosf(a2) * r, cy + sinf(a2) * r);
        draw->AddTriangleFilled(ImVec2(cx, cy), p1, p2,
            IM_COL32(252, 111, 41, alpha));
    }

    // Data nodes
    constexpr int nodeCount = 8;
    for (int i = 0; i < nodeCount; ++i) {
        float seed = (float)(i * 137 + 42);
        float nx = panelX + panelW * (0.15f + 0.7f * (sinf(seed) * 0.5f + 0.5f));
        float ny = panelY + panelH * (0.15f + 0.7f * (cosf(seed * 1.7f) * 0.5f + 0.5f));

        float pulse = sinf(globalTime * 2.0f + seed) * 0.5f + 0.5f;
        uint8_t nodeAlpha = (uint8_t)(std::min(40.0f + pulse * 80.0f, 255.0f));
        float nodeR = 2.0f + pulse * 2.0f;

        draw->AddCircleFilled(ImVec2(nx, ny), nodeR,
            IM_COL32(252, 111, 41, nodeAlpha));

        if (pulse > 0.6f) {
            draw->AddLine(ImVec2(nx, ny), ImVec2(cx, cy),
                IM_COL32(252, 111, 41, (uint8_t)(std::min(20.0f * pulse, 255.0f))), 1.0f);
        }
    }

    // Top-right decorative text
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    char statusBuf[64];
    snprintf(statusBuf, sizeof(statusBuf), "SYS.TIME: %.1f", globalTime);
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 15.0f),
        IM_COL32(16, 13, 10, 200), statusBuf);
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 30.0f),
        IM_COL32(16, 13, 10, 180), "STATUS: STANDBY");
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 45.0f),
        IM_COL32(16, 13, 10, 160), "ENCRYPTION: AES-256");

    if (smallFont) ImGui::PopFont();
}

// ============================================================
// RenderSplashScreen
// ============================================================

void RenderSplashScreen(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    ui.splashTimer += dt;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    // Game title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = vpPos.x + (vpSize.x - titleSize.x) * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.35f;

    draw->AddText(ImVec2(titleX, titleY), IM_COL32(16, 13, 10, 80), title);
    draw->AddText(ImVec2(titleX - 1, titleY), IM_COL32(16, 13, 10, 140), title);
    draw->AddText(ImVec2(titleX, titleY), IM_COL32(16, 13, 10, 255), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* bodyFont = UITheme::GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* subtitle = "TACTICAL NETWORK INFILTRATION SYSTEM";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    float subX = vpPos.x + (vpSize.x - subSize.x) * 0.5f;
    float subY = titleY + titleSize.y + 12.0f;
    draw->AddText(ImVec2(subX, subY), IM_COL32(16, 13, 10, 220), subtitle);

    if (bodyFont) ImGui::PopFont();

    // ">> PRESS ANY KEY TO INITIALIZE <<" — blinking (black)
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* prompt = ">> PRESS ANY KEY TO INITIALIZE <<";
    float blinkAlpha = (sinf(ui.splashTimer * UITheme::kPI * 2.0f) + 1.0f) * 0.5f;
    uint8_t promptAlpha = (uint8_t)(std::min(blinkAlpha * 200.0f + 55.0f, 255.0f));
    ImVec2 promptSize = ImGui::CalcTextSize(prompt);
    float promptX = vpPos.x + (vpSize.x - promptSize.x) * 0.5f;
    float promptY = vpPos.y + vpSize.y * 0.62f;
    draw->AddText(ImVec2(promptX, promptY),
        IM_COL32(16, 13, 10, promptAlpha), prompt);

    if (termFont) ImGui::PopFont();

    // Bottom team info
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    const char* team = "TEAM 08 // CSC8507 ADVANCED GAME TECHNOLOGIES // 2025";
    ImVec2 teamSize = ImGui::CalcTextSize(team);
    float teamX = vpPos.x + (vpSize.x - teamSize.x) * 0.5f;
    float teamY = vpPos.y + vpSize.y - 40.0f;
    draw->AddText(ImVec2(teamX, teamY), IM_COL32(16, 13, 10, 200), team);

    if (smallFont) ImGui::PopFont();

    // Decorative line
    float lineY = subY + 30.0f;
    float lineHalfW = 120.0f;
    float cxLine = vpPos.x + vpSize.x * 0.5f;
    draw->AddLine(ImVec2(cxLine - lineHalfW, lineY), ImVec2(cxLine + lineHalfW, lineY),
        IM_COL32(200, 200, 200, 120), 1.0f);

    ImGui::End();

    // Detect any key/mouse press -> switch to MainMenu
    if (ui.splashTimer > 0.5f) {
        bool anyInput = false;

        const Keyboard* kb = Window::GetKeyboard();
        if (kb) {
            // 从 BACK(0x08) 开始扫描，跳过 0x00-0x07 的鼠标虚拟键码
            for (int k = (int)KeyCodes::BACK; k < (int)KeyCodes::MAXVALUE; ++k) {
                if (kb->KeyPressed(static_cast<KeyCodes::Type>(k))) {
                    anyInput = true;
                    break;
                }
            }
        }

        if (!anyInput) {
            const Mouse* mouse = Window::GetMouse();
            if (mouse && (mouse->ButtonPressed(NCL::MouseButtons::Left) ||
                          mouse->ButtonPressed(NCL::MouseButtons::Right))) {
                anyInput = true;
            }
        }

        if (anyInput) {
            ui.previousScreen = ui.activeScreen;
            ui.activeScreen = UIScreen::MainMenu;
            ui.menuSelectedIndex = 0;
            LOG_INFO("[UI_Menus] Splash -> MainMenu");
        }
    }
}

// ============================================================
// RenderMainMenu
// ============================================================

void RenderMainMenu(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##MainMenu", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    // Layout
    float leftPanelW = vpSize.x * 0.35f;
    float rightPanelX = vpPos.x + leftPanelW;
    float rightPanelW = vpSize.x * 0.65f;

    // Right panel: tactical background animation
    RenderMenuBackground(ui.globalTime, rightPanelX, vpPos.y, rightPanelW, vpSize.y);

    // Divider line
    draw->AddLine(
        ImVec2(vpPos.x + leftPanelW, vpPos.y + 40.0f),
        ImVec2(vpPos.x + leftPanelW, vpPos.y + vpSize.y - 40.0f),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // Left panel
    float panelPadX = 40.0f;
    float startY = vpPos.y + 60.0f;

    // Game title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    draw->AddText(ImVec2(vpPos.x + panelPadX, startY),
        IM_COL32(16, 13, 10, 255), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    float subtitleY = startY + 36.0f;
    draw->AddText(ImVec2(vpPos.x + panelPadX, subtitleY),
        IM_COL32(16, 13, 10, 200), "TACTICAL INFILTRATION v1.0");

    // Decorative line
    float lineY = subtitleY + 22.0f;
    draw->AddLine(
        ImVec2(vpPos.x + panelPadX, lineY),
        ImVec2(vpPos.x + panelPadX + 200.0f, lineY),
        IM_COL32(200, 200, 200, 120), 1.0f);

    if (smallFont) ImGui::PopFont();

    // Menu items
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = lineY + 30.0f;
    float menuItemH  = 34.0f;

    // Keyboard navigation
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.menuSelectedIndex = (ui.menuSelectedIndex - 1 + kMenuItemCount) % kMenuItemCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.menuSelectedIndex = (ui.menuSelectedIndex + 1) % kMenuItemCount;
        }
    }

    // Mouse hover
    const Mouse* mouse = Window::GetMouse();
    for (int i = 0; i < kMenuItemCount; ++i) {
        ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.menuSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    // Confirm detection
    int8_t confirmedIndex = -1;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.menuSelectedIndex;
    }
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        for (int i = 0; i < kMenuItemCount; ++i) {
            ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, menuStartY + i * menuItemH - 2.0f);
            ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                confirmedIndex = static_cast<int8_t>(i);
                break;
            }
        }
    }

    for (int i = 0; i < kMenuItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.menuSelectedIndex);

        float offsetX = isSelected ? 4.0f : 0.0f;
        float baseX = vpPos.x + panelPadX + offsetX;

        ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, itemY - 2.0f);
        ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, itemY + menuItemH - 6.0f);

        // Selected item highlight (orange accent)
        if (isSelected) {
            draw->AddRectFilled(itemMin, itemMax,
                IM_COL32(252, 111, 41, 25), 2.0f);
            draw->AddRect(itemMin, itemMax,
                IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);
        }

        char buf[128];
        if (isSelected) {
            snprintf(buf, sizeof(buf), "> %s", kMenuItems[i]);
        } else {
            snprintf(buf, sizeof(buf), "  %s", kMenuItems[i]);
        }

        ImU32 textColor = isSelected ? IM_COL32(16, 13, 10, 255)
                                     : IM_COL32(16, 13, 10, 220);
        draw->AddText(ImVec2(baseX, itemY), textColor, buf);
    }

    if (termFont) ImGui::PopFont();

    // Confirm action
    if (confirmedIndex >= 0) {
        switch (confirmedIndex) {
            case 0: // START OPERATION
                ui.pendingSceneRequest = SceneRequest::StartGame;
                LOG_INFO("[UI_Menus] MainMenu -> StartGame");
                break;
            case 1: // MULTIPLAYER
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Lobby;
                ui.lobbySelectedIndex = 0;
                LOG_INFO("[UI_Menus] MainMenu -> Lobby");
                break;
            case 2: // LOADOUT
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Loadout;
                ui.loadoutSelectedIndex = 0;
                LOG_INFO("[UI_Menus] MainMenu -> Loadout");
                break;
            case 3: // SETTINGS
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Settings;
                LOG_INFO("[UI_Menus] MainMenu -> Settings");
                break;
            case 4: // TEAM
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Team;
                ui.teamStartTime = 0.0f;
                LOG_INFO("[UI_Menus] MainMenu -> Team");
                break;
            case 5: // EXIT
                ui.pendingSceneRequest = SceneRequest::QuitApp;
                LOG_INFO("[UI_Menus] MainMenu -> QuitApp");
                break;
            default:
                break;
        }
    }

    // Bottom status bar
    if (smallFont) ImGui::PushFont(smallFont);

    float bottomY = vpPos.y + vpSize.y - 35.0f;
    draw->AddLine(
        ImVec2(vpPos.x + panelPadX, bottomY - 8.0f),
        ImVec2(vpPos.x + leftPanelW - 20.0f, bottomY - 8.0f),
        IM_COL32(200, 200, 200, 80), 1.0f);

    draw->AddText(ImVec2(vpPos.x + panelPadX, bottomY),
        IM_COL32(16, 13, 10, 200),
        "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] BACK");

    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

// ============================================================
// RenderSettingsScreen
// ============================================================

void RenderSettingsScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##Settings", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    // Settings panel centered (responsive to viewport)
    float panelW = std::min(500.0f, vpSize.x * 0.45f);
    float panelH = std::min(460.0f, vpSize.y * 0.70f);
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // Panel background #F5EEE8
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(245, 238, 232, 255), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(200, 200, 200, 120), 3.0f, 0, 1.0f);

    float contentX = panelX + 30.0f;
    float contentY = panelY + 25.0f;

    // Title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(contentX, contentY), IM_COL32(16, 13, 10, 255), "SETTINGS");
    if (titleFont) ImGui::PopFont();

    contentY += 45.0f;

    // Separator
    draw->AddLine(
        ImVec2(contentX, contentY),
        ImVec2(panelX + panelW - 30.0f, contentY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    contentY += 20.0f;

    // ImGui controls
    ImGui::SetCursorScreenPos(ImVec2(contentX, contentY));

    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    ImGui::PushItemWidth(200.0f);

    // Display section
    ImGui::TextColored(ImVec4(0.063f, 0.051f, 0.039f, 1.0f), "DISPLAY");
    ImGui::Spacing();

    // Build label array from centralized presets
    const char* resLabels[kResolutionCount];
    for (int i = 0; i < kResolutionCount; ++i) resLabels[i] = kResolutions[i].label;

    int resIdx = static_cast<int>(ui.resolutionIndex);
    ImGui::Text("Resolution:");
    ImGui::SameLine(160.0f);
    if (ui.isFullscreen) ImGui::BeginDisabled();
    if (ImGui::Combo("##Resolution", &resIdx, resLabels, kResolutionCount)) {
        ui.resolutionIndex = static_cast<int8_t>(resIdx);
        ui.resolutionChanged = true;
        LOG_INFO("[UI_Menus] Resolution changed: " << resLabels[resIdx]);
    }
    if (ui.isFullscreen) ImGui::EndDisabled();

    ImGui::Spacing();

    ImGui::Text("Fullscreen:");
    ImGui::SameLine(160.0f);
    if (ImGui::Checkbox("##Fullscreen", &ui.isFullscreen)) {
        ui.fullscreenChanged = true;
        LOG_INFO("[UI_Menus] Fullscreen request: " << (ui.isFullscreen ? "ON" : "OFF"));
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Audio section
    ImGui::TextColored(ImVec4(0.063f, 0.051f, 0.039f, 1.0f), "AUDIO");
    ImGui::Spacing();

    ImGui::Text("Master Volume:");
    ImGui::SameLine(160.0f);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.988f, 0.435f, 0.161f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.53f, 0.28f, 1.0f));
    {
        float masterPct = ui.masterVolume * 100.0f;
        if (ImGui::SliderFloat("##MasterVol", &masterPct, 0.0f, 100.0f, "%.0f%%",
                                ImGuiSliderFlags_AlwaysClamp)) {
            ui.masterVolume = masterPct / 100.0f;
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Text("SFX Volume:");
    ImGui::SameLine(160.0f);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.988f, 0.435f, 0.161f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.53f, 0.28f, 1.0f));
    {
        float sfxPct = ui.sfxVolume * 100.0f;
        if (ImGui::SliderFloat("##SFXVol", &sfxPct, 0.0f, 100.0f, "%.0f%%",
                                ImGuiSliderFlags_AlwaysClamp)) {
            ui.sfxVolume = sfxPct / 100.0f;
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Spacing();

    // Controls section
    ImGui::TextColored(ImVec4(0.063f, 0.051f, 0.039f, 1.0f), "CONTROLS");
    ImGui::Spacing();

    ImGui::Text("Mouse Sensitivity:");
    ImGui::SameLine(160.0f);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.988f, 0.435f, 0.161f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.53f, 0.28f, 1.0f));
    {
        float sensPct = ui.mouseSensitivity * 100.0f;
        if (ImGui::SliderFloat("##MouseSens", &sensPct, 0.0f, 100.0f, "%.0f%%",
                                ImGuiSliderFlags_AlwaysClamp)) {
            ui.mouseSensitivity = sensPct / 100.0f;
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::PopItemWidth();
    ImGui::Spacing();
    ImGui::Spacing();

    // BACK button（逻辑与 Sys_UI ESC 共享 NavigateBackFromSettings）
    ImGui::SetCursorScreenPos(ImVec2(contentX, panelY + panelH - 70.0f));
    if (ImGui::Button("< BACK", ImVec2(120, 35))) {
        NavigateBackFromSettings(ui);
        LOG_INFO("[UI_Menus] Settings -> BACK button");
    }

    if (termFont) ImGui::PopFont();

    ImGui::End();
}

// ============================================================
// RenderPauseMenu
// ============================================================

void RenderPauseMenu(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##PauseMenu", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Semi-transparent overlay
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 200));

    // Panel centered
    float panelW = 400.0f;
    float panelH = 280.0f;
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // Panel background #F5EEE8
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(245, 238, 232, 255), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(200, 200, 200, 120), 3.0f, 0, 1.0f);

    // Title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "PAUSED";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = panelX + (panelW - titleSize.x) * 0.5f;
    draw->AddText(ImVec2(titleX, panelY + 25.0f), IM_COL32(16, 13, 10, 255), title);

    if (titleFont) ImGui::PopFont();

    // Separator
    float lineY = panelY + 65.0f;
    draw->AddLine(
        ImVec2(panelX + 30.0f, lineY),
        ImVec2(panelX + panelW - 30.0f, lineY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // Menu items
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = lineY + 20.0f;
    float menuItemH  = 38.0f;

    // Keyboard navigation
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.pauseSelectedIndex = (ui.pauseSelectedIndex - 1 + kPauseItemCount) % kPauseItemCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.pauseSelectedIndex = (ui.pauseSelectedIndex + 1) % kPauseItemCount;
        }
    }

    // Mouse hover
    const Mouse* mouse = Window::GetMouse();
    for (int i = 0; i < kPauseItemCount; ++i) {
        ImVec2 itemMin(panelX + 30.0f, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.pauseSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    // Confirm detection
    int8_t confirmedIndex = -1;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.pauseSelectedIndex;
    }
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        for (int i = 0; i < kPauseItemCount; ++i) {
            ImVec2 itemMin(panelX + 30.0f, menuStartY + i * menuItemH - 2.0f);
            ImVec2 itemMax(panelX + panelW - 30.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                confirmedIndex = static_cast<int8_t>(i);
                break;
            }
        }
    }

    for (int i = 0; i < kPauseItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.pauseSelectedIndex);

        float baseX = panelX + 50.0f + (isSelected ? 4.0f : 0.0f);

        ImVec2 itemMin(panelX + 30.0f, itemY - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, itemY + menuItemH - 6.0f);

        if (isSelected) {
            draw->AddRectFilled(itemMin, itemMax, IM_COL32(252, 111, 41, 25), 2.0f);
            draw->AddRect(itemMin, itemMax, IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);
        }

        char buf[64];
        snprintf(buf, sizeof(buf), isSelected ? "> %s" : "  %s", kPauseItems[i]);
        ImU32 textColor = isSelected ? IM_COL32(16, 13, 10, 255)
                                     : IM_COL32(16, 13, 10, 220);
        draw->AddText(ImVec2(baseX, itemY), textColor, buf);
    }

    if (termFont) ImGui::PopFont();

    // Confirm action
    if (confirmedIndex >= 0) {
        switch (confirmedIndex) {
            case 0: // RESUME
                ui.activeScreen = ui.prePauseScreen;
                LOG_INFO("[UI_Menus] PauseMenu -> Resume");
                break;
            case 1: // SETTINGS
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Settings;
                LOG_INFO("[UI_Menus] PauseMenu -> Settings");
                break;
            case 2: // RETURN TO MENU
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[UI_Menus] PauseMenu -> ReturnToMenu");
                break;
            default:
                break;
        }
    }

    // Bottom hint
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(panelX + 30.0f, panelY + panelH - 30.0f),
        IM_COL32(16, 13, 10, 200),
        "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] RESUME");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
