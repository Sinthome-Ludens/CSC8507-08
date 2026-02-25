#include "UI_Menus.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// 静态菜单数据
// ============================================================

static const char* kMenuItems[] = {
    "START OPERATION",
    "MULTIPLAYER",
    "LEVEL SELECT",
    "LOADOUT",
    "ACHIEVEMENTS",
    "SETTINGS",
    "TEAM",
    "EXIT",
};
static constexpr int kMenuItemCount = 8;

static bool IsMenuItemEnabled(int index) {
    return index == 0 || index == 5 || index == 7; // START, SETTINGS, EXIT
}

static const char* kPauseItems[] = {
    "RESUME",
    "SETTINGS",
    "RETURN TO MENU",
};
static constexpr int kPauseItemCount = 3;

// ============================================================
// RenderMenuBackground — 战术雷达动画（右侧面板）
// ============================================================

static void RenderMenuBackground(float globalTime,
    float panelX, float panelY, float panelW, float panelH)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // 网格线
    constexpr float gridSpacing = 40.0f;
    ImU32 gridColor = IM_COL32(0, 50, 47, 35);

    for (float x = panelX; x < panelX + panelW; x += gridSpacing) {
        draw->AddLine(ImVec2(x, panelY), ImVec2(x, panelY + panelH), gridColor, 1.0f);
    }
    for (float y = panelY; y < panelY + panelH; y += gridSpacing) {
        draw->AddLine(ImVec2(panelX, y), ImVec2(panelX + panelW, y), gridColor, 1.0f);
    }

    // 中心雷达
    float cx = panelX + panelW * 0.5f;
    float cy = panelY + panelH * 0.5f;
    float maxR = panelH * 0.30f;

    // 雷达圆环
    draw->AddCircle(ImVec2(cx, cy), maxR,       IM_COL32(0, 80, 75, 40), 64, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), maxR * 0.6f, IM_COL32(0, 80, 75, 30), 48, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), maxR * 0.3f, IM_COL32(0, 80, 75, 25), 32, 1.0f);

    // 十字线
    draw->AddLine(ImVec2(cx - maxR, cy), ImVec2(cx + maxR, cy),
        IM_COL32(0, 80, 75, 30), 1.0f);
    draw->AddLine(ImVec2(cx, cy - maxR), ImVec2(cx, cy + maxR),
        IM_COL32(0, 80, 75, 30), 1.0f);

    // 旋转扫描线
    float angle = globalTime * 0.8f;
    float scanX = cx + cosf(angle) * maxR;
    float scanY = cy + sinf(angle) * maxR;
    draw->AddLine(ImVec2(cx, cy), ImVec2(scanX, scanY),
        IM_COL32(0, 200, 190, 120), 2.0f);

    // 扫描线尾迹
    constexpr int trailSegments = 12;
    constexpr float trailAngle  = 0.5f;
    for (int i = 0; i < trailSegments; ++i) {
        float t = (float)i / (float)trailSegments;
        float a = angle - trailAngle * t;
        uint8_t alpha = (uint8_t)(60.0f * (1.0f - t));
        float r = maxR;
        ImVec2 p1(cx + cosf(a) * r, cy + sinf(a) * r);
        float a2 = angle - trailAngle * (t + 1.0f / trailSegments);
        ImVec2 p2(cx + cosf(a2) * r, cy + sinf(a2) * r);
        draw->AddTriangleFilled(ImVec2(cx, cy), p1, p2,
            IM_COL32(0, 140, 130, alpha));
    }

    // 数据节点
    constexpr int nodeCount = 8;
    for (int i = 0; i < nodeCount; ++i) {
        float seed = (float)(i * 137 + 42);
        float nx = panelX + panelW * (0.15f + 0.7f * (sinf(seed) * 0.5f + 0.5f));
        float ny = panelY + panelH * (0.15f + 0.7f * (cosf(seed * 1.7f) * 0.5f + 0.5f));

        float pulse = sinf(globalTime * 2.0f + seed) * 0.5f + 0.5f;
        uint8_t nodeAlpha = (uint8_t)(40.0f + pulse * 80.0f);
        float nodeR = 2.0f + pulse * 2.0f;

        draw->AddCircleFilled(ImVec2(nx, ny), nodeR,
            IM_COL32(0, 200, 190, nodeAlpha));

        if (pulse > 0.6f) {
            draw->AddLine(ImVec2(nx, ny), ImVec2(cx, cy),
                IM_COL32(0, 120, 110, (uint8_t)(20.0f * pulse)), 1.0f);
        }
    }

    // 右上角装饰文字
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    char statusBuf[64];
    snprintf(statusBuf, sizeof(statusBuf), "SYS.TIME: %.1f", globalTime);
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 15.0f),
        IM_COL32(0, 120, 110, 120), statusBuf);
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 30.0f),
        IM_COL32(0, 120, 110, 100), "STATUS: STANDBY");
    draw->AddText(ImVec2(panelX + panelW - 180.0f, panelY + 45.0f),
        IM_COL32(0, 120, 110, 80), "ENCRYPTION: AES-256");

    if (smallFont) ImGui::PopFont();
}

// ============================================================
// RenderSplashScreen
// ============================================================

void RenderSplashScreen(Registry& registry, float dt) {
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

    // 深色背景
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(8, 10, 16, 250));

    // 游戏标题
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = vpPos.x + (vpSize.x - titleSize.x) * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.35f;

    // 发光效果
    draw->AddText(ImVec2(titleX, titleY), IM_COL32(0, 140, 130, 80), title);
    draw->AddText(ImVec2(titleX - 1, titleY), IM_COL32(0, 200, 190, 120), title);
    draw->AddText(ImVec2(titleX, titleY), IM_COL32(0, 220, 210, 255), title);

    if (titleFont) ImGui::PopFont();

    // 副标题
    ImFont* bodyFont = UITheme::GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* subtitle = "TACTICAL NETWORK INFILTRATION SYSTEM";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    float subX = vpPos.x + (vpSize.x - subSize.x) * 0.5f;
    float subY = titleY + titleSize.y + 12.0f;
    draw->AddText(ImVec2(subX, subY), IM_COL32(0, 140, 130, 180), subtitle);

    if (bodyFont) ImGui::PopFont();

    // ">> PRESS ANY KEY TO INITIALIZE <<" — 闪烁
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* prompt = ">> PRESS ANY KEY TO INITIALIZE <<";
    float blinkAlpha = (sinf(ui.splashTimer * UITheme::kPI * 2.0f) + 1.0f) * 0.5f;
    uint8_t promptAlpha = (uint8_t)(blinkAlpha * 200.0f + 55.0f);
    ImVec2 promptSize = ImGui::CalcTextSize(prompt);
    float promptX = vpPos.x + (vpSize.x - promptSize.x) * 0.5f;
    float promptY = vpPos.y + vpSize.y * 0.62f;
    draw->AddText(ImVec2(promptX, promptY),
        IM_COL32(0, 220, 210, promptAlpha), prompt);

    if (termFont) ImGui::PopFont();

    // 底部团队信息
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    const char* team = "TEAM 08 // CSC8507 ADVANCED GAME TECHNOLOGIES // 2025";
    ImVec2 teamSize = ImGui::CalcTextSize(team);
    float teamX = vpPos.x + (vpSize.x - teamSize.x) * 0.5f;
    float teamY = vpPos.y + vpSize.y - 40.0f;
    draw->AddText(ImVec2(teamX, teamY), IM_COL32(80, 90, 95, 180), team);

    if (smallFont) ImGui::PopFont();

    // 装饰线
    float lineY = subY + 30.0f;
    float lineHalfW = 120.0f;
    float cx = vpPos.x + vpSize.x * 0.5f;
    draw->AddLine(ImVec2(cx - lineHalfW, lineY), ImVec2(cx + lineHalfW, lineY),
        IM_COL32(0, 140, 130, 100), 1.0f);

    ImGui::End();

    // 检测任意键/鼠标按下 → 切换到MainMenu
    if (ui.splashTimer > 0.5f) {
        bool anyInput = false;

        const Keyboard* kb = Window::GetKeyboard();
        if (kb) {
            for (int k = 0; k < KeyCodes::MAXVALUE; ++k) {
                if (kb->KeyPressed(static_cast<KeyCodes::Type>(k)) && k >= KeyCodes::BACK) {
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

    // 深色背景
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(6, 8, 14, 252));

    // 布局参数
    float leftPanelW = vpSize.x * 0.35f;
    float rightPanelX = vpPos.x + leftPanelW;
    float rightPanelW = vpSize.x * 0.65f;

    // 右侧面板：战术背景动画
    RenderMenuBackground(ui.globalTime, rightPanelX, vpPos.y, rightPanelW, vpSize.y);

    // 分隔线
    draw->AddLine(
        ImVec2(vpPos.x + leftPanelW, vpPos.y + 40.0f),
        ImVec2(vpPos.x + leftPanelW, vpPos.y + vpSize.y - 40.0f),
        IM_COL32(0, 100, 95, 80), 1.0f);

    // 左侧面板
    float panelPadX = 40.0f;
    float startY = vpPos.y + 60.0f;

    // 游戏标题
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    draw->AddText(ImVec2(vpPos.x + panelPadX, startY),
        IM_COL32(0, 220, 210, 255), title);

    if (titleFont) ImGui::PopFont();

    // 副标题
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    float subtitleY = startY + 36.0f;
    draw->AddText(ImVec2(vpPos.x + panelPadX, subtitleY),
        IM_COL32(0, 140, 130, 150), "TACTICAL INFILTRATION v1.0");

    // 装饰线
    float lineY = subtitleY + 22.0f;
    draw->AddLine(
        ImVec2(vpPos.x + panelPadX, lineY),
        ImVec2(vpPos.x + panelPadX + 200.0f, lineY),
        IM_COL32(0, 140, 130, 100), 1.0f);

    if (smallFont) ImGui::PopFont();

    // 菜单项
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = lineY + 30.0f;
    float menuItemH  = 34.0f;

    // 键盘导航
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.menuSelectedIndex = (ui.menuSelectedIndex - 1 + kMenuItemCount) % kMenuItemCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.menuSelectedIndex = (ui.menuSelectedIndex + 1) % kMenuItemCount;
        }
    }

    // 先处理鼠标 hover 更新 selectedIndex，再统一检测确认
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

    // 确认检测（keyboard / mouse 统一在 hover 更新后执行）
    int8_t confirmedIndex = -1;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.menuSelectedIndex;
    }
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        confirmedIndex = ui.menuSelectedIndex;
    }

    for (int i = 0; i < kMenuItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.menuSelectedIndex);
        bool isEnabled  = IsMenuItemEnabled(i);

        float offsetX = isSelected ? 4.0f : 0.0f;
        float baseX = vpPos.x + panelPadX + offsetX;

        ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, itemY - 2.0f);
        ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, itemY + menuItemH - 6.0f);

        // 选中项高亮背景
        if (isSelected) {
            draw->AddRectFilled(itemMin, itemMax,
                IM_COL32(0, 80, 75, 40), 2.0f);
            draw->AddRect(itemMin, itemMax,
                IM_COL32(0, 180, 170, 60), 2.0f, 0, 1.0f);
        }

        // 构造显示文本
        char buf[128];
        if (isSelected) {
            snprintf(buf, sizeof(buf), "> %s", kMenuItems[i]);
        } else {
            snprintf(buf, sizeof(buf), "  %s", kMenuItems[i]);
        }

        ImU32 textColor;
        if (!isEnabled) {
            textColor = IM_COL32(60, 65, 70, 180);
        } else if (isSelected) {
            textColor = IM_COL32(0, 220, 210, 255);
        } else {
            textColor = IM_COL32(130, 140, 150, 220);
        }

        draw->AddText(ImVec2(baseX, itemY), textColor, buf);

        // 不可用项显示 "COMING SOON"
        if (!isEnabled && isSelected) {
            ImVec2 tagSize = ImGui::CalcTextSize("[COMING SOON]");
            float tagX = itemMax.x - tagSize.x - 5.0f;
            draw->AddText(ImVec2(tagX, itemY),
                IM_COL32(90, 70, 20, 180), "[COMING SOON]");
        }
    }

    if (termFont) ImGui::PopFont();

    // 确认操作
    if (confirmedIndex >= 0 && IsMenuItemEnabled(confirmedIndex)) {
        switch (confirmedIndex) {
            case 0: // START OPERATION
                ui.pendingSceneRequest = SceneRequest::StartGame;
                LOG_INFO("[UI_Menus] MainMenu -> StartGame");
                break;
            case 5: // SETTINGS
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Settings;
                LOG_INFO("[UI_Menus] MainMenu -> Settings");
                break;
            case 7: // EXIT
                ui.pendingSceneRequest = SceneRequest::QuitApp;
                LOG_INFO("[UI_Menus] MainMenu -> QuitApp");
                break;
            default:
                break;
        }
    }

    // 底部状态栏
    if (smallFont) ImGui::PushFont(smallFont);

    float bottomY = vpPos.y + vpSize.y - 35.0f;
    draw->AddLine(
        ImVec2(vpPos.x + panelPadX, bottomY - 8.0f),
        ImVec2(vpPos.x + leftPanelW - 20.0f, bottomY - 8.0f),
        IM_COL32(0, 100, 95, 60), 1.0f);

    draw->AddText(ImVec2(vpPos.x + panelPadX, bottomY),
        IM_COL32(60, 70, 75, 150),
        "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] BACK");

    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

// ============================================================
// RenderSettingsScreen
// ============================================================

void RenderSettingsScreen(Registry& registry, float /*dt*/) {
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

    // 深色背景
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(6, 8, 14, 252));

    // 设置面板居中
    float panelW = 500.0f;
    float panelH = 350.0f;
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // 面板背景
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(10, 14, 20, 240), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(0, 140, 130, 100), 3.0f, 0, 1.0f);

    float contentX = panelX + 30.0f;
    float contentY = panelY + 25.0f;

    // 标题
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(contentX, contentY), IM_COL32(0, 220, 210, 255), "SETTINGS");
    if (titleFont) ImGui::PopFont();

    contentY += 45.0f;

    // 分隔线
    draw->AddLine(
        ImVec2(contentX, contentY),
        ImVec2(panelX + panelW - 30.0f, contentY),
        IM_COL32(0, 140, 130, 80), 1.0f);

    contentY += 20.0f;

    // 使用ImGui控件
    ImGui::SetCursorScreenPos(ImVec2(contentX, contentY));

    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    ImGui::PushItemWidth(200.0f);

    // 分辨率选项
    ImGui::TextColored(ImVec4(0.0f, 0.85f, 0.80f, 1.0f), "DISPLAY");
    ImGui::Spacing();

    const char* resolutions[] = {"1280 x 720", "1920 x 1080"};
    int resIdx = static_cast<int>(ui.resolutionIndex);
    ImGui::Text("Resolution:");
    ImGui::SameLine(160.0f);
    if (ImGui::Combo("##Resolution", &resIdx, resolutions, 2)) {
        ui.resolutionIndex = static_cast<int8_t>(resIdx);
        LOG_INFO("[UI_Menus] Resolution selection: " << resolutions[resIdx]);
    }

    ImGui::Spacing();

    // 全屏切换
    ImGui::Text("Fullscreen:");
    ImGui::SameLine(160.0f);
    if (ImGui::Checkbox("##Fullscreen", &ui.isFullscreen)) {
        ui.fullscreenChanged = true;
        LOG_INFO("[UI_Menus] Fullscreen request: " << (ui.isFullscreen ? "ON" : "OFF"));
    }

    ImGui::PopItemWidth();
    ImGui::Spacing();
    ImGui::Spacing();

    // BACK 按钮
    ImGui::SetCursorScreenPos(ImVec2(contentX, panelY + panelH - 70.0f));
    if (ImGui::Button("< BACK", ImVec2(120, 35))) {
        UIScreen returnTo = (ui.previousScreen == UIScreen::PauseMenu)
                            ? UIScreen::PauseMenu : UIScreen::MainMenu;
        ui.activeScreen = returnTo;
        ui.previousScreen = UIScreen::Settings;
        LOG_INFO("[UI_Menus] Settings -> " << (int)returnTo);
    }

    if (termFont) ImGui::PopFont();

    ImGui::End();
}

// ============================================================
// RenderPauseMenu
// ============================================================

void RenderPauseMenu(Registry& registry, float /*dt*/) {
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

    // 半透明暗化背景
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(4, 6, 10, 200));

    // 面板居中
    float panelW = 400.0f;
    float panelH = 280.0f;
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // 面板背景
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(8, 12, 18, 240), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(0, 140, 130, 100), 3.0f, 0, 1.0f);

    // 标题
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "PAUSED";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = panelX + (panelW - titleSize.x) * 0.5f;
    draw->AddText(ImVec2(titleX, panelY + 25.0f), IM_COL32(0, 220, 210, 255), title);

    if (titleFont) ImGui::PopFont();

    // 分隔线
    float lineY = panelY + 65.0f;
    draw->AddLine(
        ImVec2(panelX + 30.0f, lineY),
        ImVec2(panelX + panelW - 30.0f, lineY),
        IM_COL32(0, 140, 130, 80), 1.0f);

    // 菜单项
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = lineY + 20.0f;
    float menuItemH  = 38.0f;

    // 键盘导航
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.pauseSelectedIndex = (ui.pauseSelectedIndex - 1 + kPauseItemCount) % kPauseItemCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.pauseSelectedIndex = (ui.pauseSelectedIndex + 1) % kPauseItemCount;
        }
    }

    // 先处理鼠标 hover 更新 selectedIndex
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

    // 确认检测（统一在 hover 更新后）
    int8_t confirmedIndex = -1;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.pauseSelectedIndex;
    }
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        confirmedIndex = ui.pauseSelectedIndex;
    }

    for (int i = 0; i < kPauseItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.pauseSelectedIndex);

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
        snprintf(buf, sizeof(buf), isSelected ? "> %s" : "  %s", kPauseItems[i]);
        ImU32 textColor = isSelected ? IM_COL32(0, 220, 210, 255)
                                     : IM_COL32(130, 140, 150, 220);
        draw->AddText(ImVec2(baseX, itemY), textColor, buf);
    }

    if (termFont) ImGui::PopFont();

    // 确认操作
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

    // 底部提示
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(
        ImVec2(panelX + 30.0f, panelY + panelH - 30.0f),
        IM_COL32(60, 70, 75, 150),
        "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] RESUME");

    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
