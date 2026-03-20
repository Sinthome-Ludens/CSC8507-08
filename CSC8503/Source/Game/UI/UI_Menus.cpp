/**
 * @file UI_Menus.cpp
 * @brief 通用菜单 UI 系统。
 */
#include "UI_Menus.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Anim.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_UIKeyConfig.h"

using namespace NCL;
using namespace ECS::UITheme;

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
    "TUTORIAL",
    "MULTIPLAYER",
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
    ImU32 gridColor = Col32_Gray(30);

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
    draw->AddCircle(ImVec2(cx, cy), maxR,        Col32_Accent(30), 64, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), maxR * 0.6f, Col32_Accent(25), 48, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), maxR * 0.3f, Col32_Accent(20), 32, 1.0f);

    // Crosshairs
    draw->AddLine(ImVec2(cx - maxR, cy), ImVec2(cx + maxR, cy),
        Col32_Accent(25), 1.0f);
    draw->AddLine(ImVec2(cx, cy - maxR), ImVec2(cx, cy + maxR),
        Col32_Accent(25), 1.0f);

    // Primary rotating sweep line
    float angle = globalTime * 0.8f;
    float scanX = cx + cosf(angle) * maxR;
    float scanY = cy + sinf(angle) * maxR;
    draw->AddLine(ImVec2(cx, cy), ImVec2(scanX, scanY),
        Col32_Accent(100), 2.0f);

    // Secondary sweep line (180° offset, dimmer)
    float angle2 = angle + 3.14159f;
    float scan2X = cx + cosf(angle2) * maxR;
    float scan2Y = cy + sinf(angle2) * maxR;
    draw->AddLine(ImVec2(cx, cy), ImVec2(scan2X, scan2Y),
        Col32_Accent(40), 1.5f);

    // Sweep trail (primary)
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
            Col32_Accent(alpha));
    }

    // Sweep trail (secondary, dimmer)
    for (int i = 0; i < 8; ++i) {
        float t = (float)i / 8.0f;
        float a = angle2 - 0.3f * t;
        uint8_t alpha = (uint8_t)(std::min(25.0f * (1.0f - t), 255.0f));
        ImVec2 p1(cx + cosf(a) * maxR, cy + sinf(a) * maxR);
        float a2 = angle2 - 0.3f * (t + 1.0f / 8.0f);
        ImVec2 p2(cx + cosf(a2) * maxR, cy + sinf(a2) * maxR);
        draw->AddTriangleFilled(ImVec2(cx, cy), p1, p2,
            Col32_Accent(alpha));
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
            Col32_Accent(nodeAlpha));

        if (pulse > 0.6f) {
            draw->AddLine(ImVec2(nx, ny), ImVec2(cx, cy),
                Col32_Accent((uint8_t)(std::min(20.0f * pulse, 255.0f))), 1.0f);
        }
    }

    // Top-right decorative text
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    char statusBuf[64];
    snprintf(statusBuf, sizeof(statusBuf), "SYS.TIME: %.1f", globalTime);
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 15.0f),
        Col32_Text(200), statusBuf);
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 30.0f),
        Col32_Text(180), "STATUS: STANDBY");
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 45.0f),
        Col32_Text(160), "ENCRYPTION: AES-256");

    // Vertical data flow text (scrolling upward on left edge)
    static const char* kDataLines[] = {
        "0xA3F1:OK", "NODE.17", "PING 12ms", "TCP/443",
        "HASH:5E9C", "AUTH:RSA", "SYNC.OK", "0xFF02:RX",
        "PKT:1024", "ROUTE:3", "DNS.OK", "TLS1.3",
    };
    constexpr int kDataLineCount = 12;
    constexpr float lineH = 14.0f;
    float scrollOffset = fmodf(globalTime * 20.0f, lineH * kDataLineCount);
    float dataX = panelX + 10.0f;
    // Clip to panel region
    draw->PushClipRect(ImVec2(panelX, panelY), ImVec2(panelX + 90.0f, panelY + panelH));
    for (int i = 0; i < kDataLineCount + 1; ++i) {
        float y = panelY + panelH - (i * lineH) + scrollOffset;
        if (y < panelY - lineH || y > panelY + panelH) continue;
        int idx = i % kDataLineCount;
        // Fade at edges
        float distTop = y - panelY;
        float distBot = (panelY + panelH) - y;
        float edgeFade = std::min(distTop, distBot) / 30.0f;
        edgeFade = std::clamp(edgeFade, 0.0f, 1.0f);
        uint8_t alpha = (uint8_t)(80.0f * edgeFade);
        draw->AddText(ImVec2(dataX, y), Col32_Accent(alpha), kDataLines[idx]);
    }
    draw->PopClipRect();

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

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg());

    // Game title
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = vpPos.x + (vpSize.x - titleSize.x) * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.35f;

    draw->AddText(ImVec2(titleX, titleY), Col32_Text(80), title);
    draw->AddText(ImVec2(titleX - 1, titleY), Col32_Text(140), title);
    draw->AddText(ImVec2(titleX, titleY), Col32_Text(255), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* bodyFont = GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* subtitle = "TACTICAL NETWORK INFILTRATION SYSTEM";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    float subX = vpPos.x + (vpSize.x - subSize.x) * 0.5f;
    float subY = titleY + titleSize.y + 12.0f;
    draw->AddText(ImVec2(subX, subY), Col32_Text(220), subtitle);

    if (bodyFont) ImGui::PopFont();

    // ">> PRESS ANY KEY TO INITIALIZE <<" — blinking
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* prompt = ">> PRESS ANY KEY TO INITIALIZE <<";
    float blinkAlpha = (sinf(ui.splashTimer * kPI * 2.0f) + 1.0f) * 0.5f;
    uint8_t promptAlpha = (uint8_t)(std::min(blinkAlpha * 200.0f + 55.0f, 255.0f));
    ImVec2 promptSize = ImGui::CalcTextSize(prompt);
    float promptX = vpPos.x + (vpSize.x - promptSize.x) * 0.5f;
    float promptY = vpPos.y + vpSize.y * 0.62f;
    draw->AddText(ImVec2(promptX, promptY),
        Col32_Text(promptAlpha), prompt);

    if (termFont) ImGui::PopFont();

    // Bottom team info
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    const char* team = "TEAM 08 // CSC8507 ADVANCED GAME TECHNOLOGIES // 2025";
    ImVec2 teamSize = ImGui::CalcTextSize(team);
    float teamX = vpPos.x + (vpSize.x - teamSize.x) * 0.5f;
    float teamY = vpPos.y + vpSize.y - 40.0f;
    draw->AddText(ImVec2(teamX, teamY), Col32_Text(200), team);

    if (smallFont) ImGui::PopFont();

    // Decorative line
    float lineY = subY + 30.0f;
    float lineHalfW = 120.0f;
    float cxLine = vpPos.x + vpSize.x * 0.5f;
    draw->AddLine(ImVec2(cxLine - lineHalfW, lineY), ImVec2(cxLine + lineHalfW, lineY),
        Col32_Gray(120), 1.0f);

    // Military-style corner frames
    {
        float margin = 30.0f;
        float cornerLen = 40.0f;
        ImU32 cornerCol = Col32_Accent(60);
        float cw = 1.5f;
        ImVec2 tl(vpPos.x + margin, vpPos.y + margin);
        ImVec2 br(vpPos.x + vpSize.x - margin, vpPos.y + vpSize.y - margin);
        // Top-left
        draw->AddLine(tl, ImVec2(tl.x + cornerLen, tl.y), cornerCol, cw);
        draw->AddLine(tl, ImVec2(tl.x, tl.y + cornerLen), cornerCol, cw);
        // Top-right
        draw->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x - cornerLen, tl.y), cornerCol, cw);
        draw->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + cornerLen), cornerCol, cw);
        // Bottom-left
        draw->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x + cornerLen, br.y), cornerCol, cw);
        draw->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x, br.y - cornerLen), cornerCol, cw);
        // Bottom-right
        draw->AddLine(br, ImVec2(br.x - cornerLen, br.y), cornerCol, cw);
        draw->AddLine(br, ImVec2(br.x, br.y - cornerLen), cornerCol, cw);
    }

    // Horizontal scan line (sweeps down slowly)
    {
        float scanPeriod = 4.0f;
        float scanT = fmodf(ui.splashTimer, scanPeriod) / scanPeriod;
        float scanLineY = vpPos.y + vpSize.y * scanT;
        uint8_t scanAlpha = (uint8_t)(40.0f * (1.0f - fabsf(scanT - 0.5f) * 2.0f));
        draw->AddLine(ImVec2(vpPos.x, scanLineY), ImVec2(vpPos.x + vpSize.x, scanLineY),
            Col32_Accent(scanAlpha), 1.0f);
    }

    ImGui::End();

    // Detect any key/mouse press -> switch to MainMenu
    if (ui.splashTimer > 0.5f) {
        bool anyInput = false;

        const auto& input = registry.ctx<Res_Input>();
        {
            for (int k = (int)KeyCodes::BACK; k < (int)KeyCodes::MAXVALUE; ++k) {
                if (input.keyPressed[static_cast<KeyCodes::Type>(k)]) {
                    anyInput = true;
                    break;
                }
            }
        }

        if (!anyInput) {
            for (size_t m = 0; m < std::size(input.mouseButtonPressed); ++m) {
                if (input.mouseButtonPressed[m]) { anyInput = true; break; }
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

void RenderMainMenu(Registry& registry, float dt) {
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

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg());

    // Entry animation progress
    float entryRaw = (ui.screenEntryDuration > 0.0f)
        ? std::clamp(ui.screenEntryElapsed / ui.screenEntryDuration, 0.0f, 1.0f) : 1.0f;
    float entryT = Anim::EaseOutCubic(entryRaw);

    // Layout
    float leftPanelW = vpSize.x * 0.35f;
    float rightPanelX = vpPos.x + leftPanelW;
    float rightPanelW = vpSize.x * 0.65f;

    // Slide transition offset (direction-dependent)
    float slideX = Anim::SlideOffset(entryT, ui.transDirection);

    // Left panel slide offset (-40→0) + transition slide
    float leftSlideX = -40.0f * (1.0f - entryT) + slideX;

    // Right panel: tactical background animation (fade in via alpha)
    // Right panel alpha increases 0→1 during entry
    {
        uint8_t rpAlpha = (uint8_t)(entryT * 255);
        // We draw the right panel background as a semi-transparent layer during entry
        if (entryT < 0.99f) {
            // Draw a bg rect over the right panel to fade it
            draw->AddRectFilled(
                ImVec2(rightPanelX, vpPos.y),
                ImVec2(rightPanelX + rightPanelW, vpPos.y + vpSize.y),
                Col32_Bg());
        }
    }
    RenderMenuBackground(ui.globalTime, rightPanelX, vpPos.y, rightPanelW, vpSize.y);

    // Divider line
    draw->AddLine(
        ImVec2(vpPos.x + leftPanelW + leftSlideX, vpPos.y + 40.0f),
        ImVec2(vpPos.x + leftPanelW + leftSlideX, vpPos.y + vpSize.y - 40.0f),
        Col32_Gray(100), 1.0f);

    // Left panel
    float panelPadX = Layout::kPagePadX + leftSlideX;
    float startY = vpPos.y + 60.0f;

    // Game title
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    draw->AddText(ImVec2(vpPos.x + panelPadX, startY),
        Col32_Text(255), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    float subtitleY = startY + 36.0f;
    draw->AddText(ImVec2(vpPos.x + panelPadX, subtitleY),
        Col32_Text(200), "TACTICAL INFILTRATION v1.0");

    // Decorative line
    float lineY = subtitleY + 22.0f;
    draw->AddLine(
        ImVec2(vpPos.x + panelPadX, lineY),
        ImVec2(vpPos.x + panelPadX + 200.0f, lineY),
        Col32_Gray(120), 1.0f);

    if (smallFont) ImGui::PopFont();

    // Menu items
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = lineY + 30.0f;
    float menuItemH  = Layout::kMenuItemH;

    // Keyboard navigation
    const auto& input = registry.ctx<Res_Input>();
    Res_UIKeyConfig defaultUiCfg;
    const auto& uiCfg = registry.has_ctx<Res_UIKeyConfig>() ? registry.ctx<Res_UIKeyConfig>() : defaultUiCfg;
    {
        if (input.keyPressed[uiCfg.keyMenuUp] || input.keyPressed[uiCfg.keyMenuUpAlt]) {
            ui.menuSelectedIndex = (ui.menuSelectedIndex - 1 + kMenuItemCount) % kMenuItemCount;
        }
        if (input.keyPressed[uiCfg.keyMenuDown] || input.keyPressed[uiCfg.keyMenuDownAlt]) {
            ui.menuSelectedIndex = (ui.menuSelectedIndex + 1) % kMenuItemCount;
        }
    }

    // Mouse hover
    for (int i = 0; i < kMenuItemCount; ++i) {
        ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.menuSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    // Confirm detection
    int8_t confirmedIndex = -1;
    if (input.keyPressed[uiCfg.keyConfirm] || input.keyPressed[uiCfg.keyConfirmAlt]) {
        confirmedIndex = ui.menuSelectedIndex;
    }
    if (input.mouseButtonPressed[uiCfg.mouseConfirm]) {
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

    // Update hover progress (SmoothLerp per item)
    for (int i = 0; i < kMenuItemCount; ++i) {
        float target = (i == ui.menuSelectedIndex) ? 1.0f : 0.0f;
        ui.menuHoverProgress[i] = Anim::SmoothLerp(ui.menuHoverProgress[i], target, 10.0f, dt);
    }

    for (int i = 0; i < kMenuItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        float hoverT = Anim::EaseOutCubic(ui.menuHoverProgress[i]);

        float offsetX = hoverT * 8.0f;
        float baseX = vpPos.x + panelPadX + offsetX;

        ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, itemY - 2.0f);
        ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, itemY + menuItemH - 6.0f);

        // Animated highlight
        uint8_t bgAlpha = (uint8_t)(hoverT * 35);
        uint8_t borderAlpha = (uint8_t)(80 + hoverT * 100);
        if (bgAlpha > 2) {
            draw->AddRectFilled(itemMin, itemMax,
                Col32_Accent(bgAlpha), 2.0f);
            draw->AddRect(itemMin, itemMax,
                Col32_Accent(borderAlpha), 2.0f, 0, Layout::kBorderWidth);
        }

        // Left orange bar (grows from center)
        if (hoverT > 0.01f) {
            float barFullH = menuItemH - 8.0f;
            float barH = hoverT * barFullH;
            float barCY = itemY + (menuItemH - 6.0f) * 0.5f - 2.0f;
            draw->AddRectFilled(
                ImVec2(itemMin.x, barCY - barH * 0.5f),
                ImVec2(itemMin.x + 3.0f, barCY + barH * 0.5f),
                Col32_Accent((uint8_t)(hoverT * 200)));
        }

        char buf[128];
        bool showCaret = (hoverT > 0.5f);
        snprintf(buf, sizeof(buf), showCaret ? "> %s" : "  %s", kMenuItems[i]);

        uint8_t textAlpha = (uint8_t)(220 + hoverT * 35);
        draw->AddText(ImVec2(baseX, itemY), Col32_Text(textAlpha), buf);

        // Click flash overlay
        if (ui.menuClickFlashIndex == i && ui.menuClickFlashTimer > 0.0f) {
            float flashT = ui.menuClickFlashTimer / 0.15f;  // 0→1 (1=just clicked)
            float flashScale = 1.0f + 0.03f * Anim::EaseOutBack(1.0f - flashT);
            uint8_t flashAlpha = (uint8_t)(180.0f * flashT);
            float halfW = (itemMax.x - itemMin.x) * 0.5f * flashScale;
            float halfH = (itemMax.y - itemMin.y) * 0.5f * flashScale;
            float midX = (itemMin.x + itemMax.x) * 0.5f;
            float midY = (itemMin.y + itemMax.y) * 0.5f;
            draw->AddRectFilled(
                ImVec2(midX - halfW, midY - halfH),
                ImVec2(midX + halfW, midY + halfH),
                IM_COL32(255, 255, 255, flashAlpha), 2.0f);
        }
    }

    if (termFont) ImGui::PopFont();

    // Confirm action
    if (confirmedIndex >= 0) {
        // Trigger click flash
        ui.menuClickFlashTimer = 0.15f;
        ui.menuClickFlashIndex = confirmedIndex;

        switch (confirmedIndex) {
            case 0: // START OPERATION
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen   = UIScreen::MissionSelect;
                ui.missionSelectedTab = 0;
                ui.missionCursorPerTab[0] = ui.missionCursorPerTab[1] = 0;
                ui.missionEquippedItems[0] = ui.missionEquippedItems[1] = -1;
                ui.missionEquippedWeapons[0] = ui.missionEquippedWeapons[1] = -1;
                LOG_INFO("[UI_Menus] MainMenu -> MissionSelect");
                break;
            case 1: // TUTORIAL
                ui.pendingSceneRequest = SceneRequest::StartTutorial;
                LOG_INFO("[UI_Menus] MainMenu -> StartTutorial");
                break;
            case 2: // MULTIPLAYER
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Lobby;
                ui.lobbySelectedIndex = 0;
                LOG_INFO("[UI_Menus] MainMenu -> Lobby");
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

    float bottomY = vpPos.y + vpSize.y - Layout::kBottomHintY;
    draw->AddLine(
        ImVec2(vpPos.x + panelPadX, bottomY - 8.0f),
        ImVec2(vpPos.x + leftPanelW - 20.0f, bottomY - 8.0f),
        Col32_Gray(80), 1.0f);

    draw->AddText(ImVec2(vpPos.x + panelPadX, bottomY),
        Col32_Text(200),
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

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg());

    // Entry animation + slide
    float entryRaw = (ui.screenEntryDuration > 0.0f)
        ? std::clamp(ui.screenEntryElapsed / ui.screenEntryDuration, 0.0f, 1.0f) : 1.0f;
    float entryT = Anim::EaseOutCubic(entryRaw);
    float slideX = Anim::SlideOffset(entryT, ui.transDirection);

    // Settings panel centered (responsive to viewport)
    float panelW = std::clamp(vpSize.x * 0.45f, 380.0f, 500.0f);
    float panelH = std::clamp(vpSize.y * 0.70f, 340.0f, 460.0f);
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f + slideX;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // Panel drop shadow
    draw->AddRectFilled(
        ImVec2(panelX + 4.0f, panelY + 4.0f),
        ImVec2(panelX + panelW + 4.0f, panelY + panelH + 4.0f),
        IM_COL32(0, 0, 0, 25), Layout::kCardRounding);

    // Panel background
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        Col32_Bg(), Layout::kCardRounding);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        Col32_Gray(120), Layout::kCardRounding, 0, Layout::kBorderWidth);

    float contentX = panelX + 30.0f;
    float contentY = panelY + 25.0f;

    // Title
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(contentX, contentY), Col32_Text(255), "SETTINGS");
    if (titleFont) ImGui::PopFont();

    contentY += 45.0f;

    // ── Tab bar ──────────────────────────────────────────────
    static const char* kTabLabels[] = { "DISPLAY", "AUDIO", "CONTROLS" };
    constexpr int kTabCount = 3;
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float tabX = contentX;
    float tabY = contentY;
    float tabGap = 10.0f;

    for (int t = 0; t < kTabCount; ++t) {
        ImVec2 labelSize = ImGui::CalcTextSize(kTabLabels[t]);
        float tabW = labelSize.x + 20.0f;
        bool isActive = (ui.settingsTab == t);
        ImVec2 tabMin(tabX, tabY);
        ImVec2 tabMax(tabX + tabW, tabY + 28.0f);

        // Click detection
        ImVec2 mousePos = ImGui::GetMousePos();
        bool hovered = (mousePos.x >= tabMin.x && mousePos.x <= tabMax.x &&
                        mousePos.y >= tabMin.y && mousePos.y <= tabMax.y);
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ui.settingsTab = (int8_t)t;
            isActive = true;
        }

        // Tab background
        if (isActive) {
            draw->AddRectFilled(tabMin, tabMax, Col32_Accent(25), 2.0f);
        } else if (hovered) {
            draw->AddRectFilled(tabMin, tabMax, Col32_Gray(20), 2.0f);
        }

        // Tab text
        draw->AddText(ImVec2(tabX + 10.0f, tabY + 5.0f),
            isActive ? Col32_Accent(255) : Col32_Text(160), kTabLabels[t]);

        // Active tab underline
        if (isActive) {
            draw->AddLine(ImVec2(tabMin.x, tabMax.y), ImVec2(tabMax.x, tabMax.y),
                Col32_Accent(200), 2.0f);
        }

        tabX += tabW + tabGap;
    }

    // Tab bar separator line
    float tabLineY = tabY + 29.0f;
    draw->AddLine(
        ImVec2(contentX, tabLineY),
        ImVec2(panelX + panelW - 30.0f, tabLineY),
        Col32_Gray(60), 1.0f);

    contentY = tabLineY + 16.0f;

    // ── Tab content ──────────────────────────────────────────
    ImGui::SetCursorScreenPos(ImVec2(contentX, contentY));
    ImGui::PushItemWidth(200.0f);

    if (ui.settingsTab == 0) {
        // ── DISPLAY tab ──────────────────────────────────────
        // Monitor icon
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float ix = cp.x - 22.0f, iy = cp.y + 2.0f;
            draw->AddRect(ImVec2(ix, iy), ImVec2(ix + 14.0f, iy + 10.0f),
                Col32_Accent(100), 1.0f, 0, 1.0f);
            draw->AddLine(ImVec2(ix + 4.0f, iy + 11.0f), ImVec2(ix + 10.0f, iy + 11.0f),
                Col32_Accent(80), 1.0f);
        }
        ImGui::TextColored(ImVec4(0.063f, 0.051f, 0.039f, 1.0f), "DISPLAY");
        ImGui::Spacing();

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
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.063f, 0.051f, 0.039f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.15f, 0.12f, 0.10f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.20f, 0.16f, 0.13f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark,       ImVec4(0.988f, 0.435f, 0.161f, 1.0f));
        if (ImGui::Checkbox("##Fullscreen", &ui.isFullscreen)) {
            ui.fullscreenChanged = true;
            LOG_INFO("[UI_Menus] Fullscreen request: " << (ui.isFullscreen ? "ON" : "OFF"));
        }
        ImGui::PopStyleColor(4);
    }
    else if (ui.settingsTab == 1) {
        // ── AUDIO tab ────────────────────────────────────────
        // Speaker icon
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float ix = cp.x - 22.0f, iy = cp.y + 2.0f;
            draw->AddRectFilled(ImVec2(ix, iy + 3.0f), ImVec2(ix + 5.0f, iy + 9.0f),
                Col32_Accent(100));
            draw->AddTriangleFilled(
                ImVec2(ix + 5.0f, iy + 1.0f), ImVec2(ix + 5.0f, iy + 11.0f),
                ImVec2(ix + 11.0f, iy + 6.0f), Col32_Accent(80));
        }
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

        ImGui::Text("BGM Volume:");
        ImGui::SameLine(160.0f);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.988f, 0.435f, 0.161f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.53f, 0.28f, 1.0f));
        {
            float bgmPct = ui.bgmVolume * 100.0f;
            if (ImGui::SliderFloat("##BGMVol", &bgmPct, 0.0f, 100.0f, "%.0f%%",
                                    ImGuiSliderFlags_AlwaysClamp)) {
                ui.bgmVolume = bgmPct / 100.0f;
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
    }
    else {
        // ── CONTROLS tab ─────────────────────────────────────
        // Crosshair icon
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float ix = cp.x - 16.0f, iy = cp.y + 6.0f;
            draw->AddCircle(ImVec2(ix, iy), 5.0f, Col32_Accent(100), 16, 1.0f);
            draw->AddLine(ImVec2(ix - 7.0f, iy), ImVec2(ix + 7.0f, iy), Col32_Accent(80), 1.0f);
            draw->AddLine(ImVec2(ix, iy - 7.0f), ImVec2(ix, iy + 7.0f), Col32_Accent(80), 1.0f);
        }
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
    }

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

void RenderPauseMenu(Registry& registry, float dt) {
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

    // Entry animation
    float entryRaw = (ui.screenEntryDuration > 0.0f)
        ? std::clamp(ui.screenEntryElapsed / ui.screenEntryDuration, 0.0f, 1.0f) : 1.0f;
    float entryT = Anim::EaseOutCubic(entryRaw);

    // Semi-transparent overlay (fade in)
    uint8_t overlayAlpha = (uint8_t)(200.0f * entryT);
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg(overlayAlpha));

    // Panel centered with scale animation (0.95→1.0)
    float scale = 0.95f + 0.05f * entryT;
    float baseW = 400.0f;
    float baseH = 280.0f;
    float panelW = baseW * scale;
    float panelH = baseH * scale;
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // Panel drop shadow
    draw->AddRectFilled(
        ImVec2(panelX + 4.0f, panelY + 4.0f),
        ImVec2(panelX + panelW + 4.0f, panelY + panelH + 4.0f),
        IM_COL32(0, 0, 0, 30), Layout::kCardRounding);

    // Panel background
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        Col32_Bg(), Layout::kCardRounding);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        Col32_Gray(120), Layout::kCardRounding, 0, Layout::kBorderWidth);

    // Title
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "PAUSED";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = panelX + (panelW - titleSize.x) * 0.5f;
    draw->AddText(ImVec2(titleX, panelY + 25.0f), Col32_Text(255), title);

    if (titleFont) ImGui::PopFont();

    // Separator
    float lineY = panelY + 65.0f;
    draw->AddLine(
        ImVec2(panelX + 30.0f, lineY),
        ImVec2(panelX + panelW - 30.0f, lineY),
        Col32_Gray(100), 1.0f);

    // Menu items
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = lineY + 20.0f;
    float menuItemH  = Layout::kPauseItemH;

    // Keyboard navigation
    const auto& input = registry.ctx<Res_Input>();
    Res_UIKeyConfig defaultUiCfg;
    const auto& uiCfg = registry.has_ctx<Res_UIKeyConfig>() ? registry.ctx<Res_UIKeyConfig>() : defaultUiCfg;
    {
        if (input.keyPressed[uiCfg.keyMenuUp] || input.keyPressed[uiCfg.keyMenuUpAlt]) {
            ui.pauseSelectedIndex = (ui.pauseSelectedIndex - 1 + kPauseItemCount) % kPauseItemCount;
        }
        if (input.keyPressed[uiCfg.keyMenuDown] || input.keyPressed[uiCfg.keyMenuDownAlt]) {
            ui.pauseSelectedIndex = (ui.pauseSelectedIndex + 1) % kPauseItemCount;
        }
    }

    // Mouse hover
    for (int i = 0; i < kPauseItemCount; ++i) {
        ImVec2 itemMin(panelX + 30.0f, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, menuStartY + i * menuItemH + menuItemH - 6.0f);
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.pauseSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    // Confirm detection
    int8_t confirmedIndex = -1;
    if (input.keyPressed[uiCfg.keyConfirm] || input.keyPressed[uiCfg.keyConfirmAlt]) {
        confirmedIndex = ui.pauseSelectedIndex;
    }
    if (input.mouseButtonPressed[uiCfg.mouseConfirm]) {
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

    // Update hover progress
    for (int i = 0; i < kPauseItemCount; ++i) {
        float target = (i == ui.pauseSelectedIndex) ? 1.0f : 0.0f;
        ui.menuHoverProgress[i] = Anim::SmoothLerp(ui.menuHoverProgress[i], target, 10.0f, dt);
    }

    for (int i = 0; i < kPauseItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        float hoverT = Anim::EaseOutCubic(ui.menuHoverProgress[i]);

        float baseX = panelX + 50.0f + hoverT * 8.0f;

        ImVec2 itemMin(panelX + 30.0f, itemY - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, itemY + menuItemH - 6.0f);

        uint8_t bgAlpha = (uint8_t)(hoverT * 35);
        uint8_t borderAlpha = (uint8_t)(80 + hoverT * 100);
        if (bgAlpha > 2) {
            draw->AddRectFilled(itemMin, itemMax, Col32_Accent(bgAlpha), 2.0f);
            draw->AddRect(itemMin, itemMax, Col32_Accent(borderAlpha), 2.0f, 0, Layout::kBorderWidth);
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

        char buf[64];
        bool showCaret = (hoverT > 0.5f);
        snprintf(buf, sizeof(buf), showCaret ? "> %s" : "  %s", kPauseItems[i]);
        uint8_t textAlpha = (uint8_t)(220 + hoverT * 35);
        draw->AddText(ImVec2(baseX, itemY), Col32_Text(textAlpha), buf);

        // Click flash overlay
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
            case 0: // RESUME
                ui.activeScreen = ui.prePauseScreen;
                if (registry.has_ctx<Res_GameState>()) {
                    registry.ctx<Res_GameState>().isPaused = false;
                }
                LOG_INFO("[UI_Menus] PauseMenu -> Resume");
                break;
            case 1: // SETTINGS
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Settings;
                LOG_INFO("[UI_Menus] PauseMenu -> Settings");
                break;
            case 2: // RETURN TO MENU
                if (registry.has_ctx<Res_GameState>()) {
                    registry.ctx<Res_GameState>().isPaused = false;
                }
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[UI_Menus] PauseMenu -> ReturnToMenu");
                break;
            default:
                break;
        }
    }

    // Bottom hint
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(panelX + 30.0f, panelY + panelH - 30.0f),
        Col32_Text(200),
        "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] RESUME");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
