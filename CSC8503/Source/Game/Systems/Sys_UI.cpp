#include "Sys_UI.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS {

// ============================================================
// OnAwake
// ============================================================

void Sys_UI::OnAwake(Registry& registry) {
    UITheme::LoadFonts();
    UITheme::ApplyCyberpunkTheme();

    if (!registry.has_ctx<Res_UIState>()) {
        registry.ctx_emplace<Res_UIState>();
    }

    LOG_INFO("[Sys_UI] OnAwake — Fonts loaded, theme applied, Res_UIState registered.");
}

// ============================================================
// OnUpdate
// ============================================================

void Sys_UI::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    ui.globalTime += dt;
    ui.menuAnimTimer += dt;

    // ── 开发者模式切换（F1）──
    const Keyboard* devKb = Window::GetKeyboard();
    if (devKb && devKb->KeyPressed(KeyCodes::F1)) {
        ui.devMode = !ui.devMode;
        LOG_INFO("[Sys_UI] DevMode: " << (ui.devMode ? "ON" : "OFF"));
    }

    // ── ESC 导航（集中处理，防止同帧 KeyPressed 双重触发）──────────
    //    KeyPressed() 在整个帧内返回 true（状态数组不变），
    //    若在 OnUpdate 和 Render 函数中分别检测 ESC 会导致同帧打开又关闭。
    //    因此所有 ESC 转场逻辑统一在此处理，Render 函数内不再检测 ESC。
    if (devKb && devKb->KeyPressed(KeyCodes::ESCAPE)) {
        switch (ui.activeScreen) {
            case UIScreen::None:
            case UIScreen::HUD:
                // 游戏场景（HUD 或无UI） → 暂停菜单
                ui.prePauseScreen = ui.activeScreen;
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::PauseMenu;
                ui.pauseSelectedIndex = 0;
                LOG_INFO("[Sys_UI] Game → PauseMenu");
                break;
            case UIScreen::PauseMenu:
                // 暂停菜单 → 恢复游戏（返回暂停前的游戏画面）
                ui.activeScreen = ui.prePauseScreen;
                LOG_INFO("[Sys_UI] PauseMenu → Resume (ESC)");
                break;
            case UIScreen::Settings: {
                // 设置 → 返回来源画面（PauseMenu 或 MainMenu）
                UIScreen returnTo = (ui.previousScreen == UIScreen::PauseMenu)
                                    ? UIScreen::PauseMenu : UIScreen::MainMenu;
                ui.activeScreen = returnTo;
                ui.previousScreen = UIScreen::Settings;
                LOG_INFO("[Sys_UI] Settings → " << (int)returnTo << " (ESC)");
                break;
            }
            case UIScreen::MainMenu:
                // 主菜单 → 退回 Splash
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Splash;
                ui.splashTimer = 0.0f;
                LOG_INFO("[Sys_UI] MainMenu → Splash (ESC)");
                break;
            default:
                break;
        }
    }

    // Dispatch 到对应画面渲染
    switch (ui.activeScreen) {
        case UIScreen::Splash:
            RenderSplashScreen(registry, dt);
            break;
        case UIScreen::MainMenu:
            RenderMainMenu(registry, dt);
            break;
        case UIScreen::Settings:
            RenderSettingsScreen(registry, dt);
            break;
        case UIScreen::PauseMenu:
            RenderPauseMenu(registry, dt);
            break;
        case UIScreen::HUD:
            RenderHUD(registry, dt);
            break;
        case UIScreen::None:
        default:
            break;
    }

    // 扫描线叠加（菜单类画面）
    if (ui.activeScreen != UIScreen::None && ui.activeScreen != UIScreen::HUD) {
        RenderScanlineOverlay(ui.globalTime);
    }

    // 更新输入阻塞标志（由 Main.cpp 读取后执行实际 Window 操作）
    ui.isUIBlockingInput = (ui.activeScreen != UIScreen::None
                         && ui.activeScreen != UIScreen::HUD);
}

// ============================================================
// OnDestroy
// ============================================================

void Sys_UI::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_UI] OnDestroy.");
}

// ============================================================
// RenderSplashScreen
// ============================================================

void Sys_UI::RenderSplashScreen(Registry& registry, float dt) {
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

    // 发光效果（绘制多层）
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
    float blinkAlpha = (sinf(ui.splashTimer * 3.14159f * 2.0f) + 1.0f) * 0.5f;
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

    // 检测任意键按下 → 切换到MainMenu
    const Keyboard* kb = Window::GetKeyboard();
    if (kb && ui.splashTimer > 0.5f) {
        for (int k = 0; k < KeyCodes::MAXVALUE; ++k) {
            if (kb->KeyPressed(static_cast<KeyCodes::Type>(k))) {
                // 排除鼠标按钮（前几个键码）
                if (k >= KeyCodes::BACK) {
                    ui.previousScreen = ui.activeScreen;
                    ui.activeScreen = UIScreen::MainMenu;
                    ui.menuSelectedIndex = 0;
                    ui.menuAnimTimer = 0.0f;
                    LOG_INFO("[Sys_UI] Splash → MainMenu");
                    break;
                }
            }
        }
    }

    // 鼠标点击也能跳过
    const Mouse* mouse = Window::GetMouse();
    if (mouse && ui.splashTimer > 0.5f) {
        if (mouse->ButtonPressed(NCL::MouseButtons::Left) ||
            mouse->ButtonPressed(NCL::MouseButtons::Right)) {
            ui.previousScreen = ui.activeScreen;
            ui.activeScreen = UIScreen::MainMenu;
            ui.menuSelectedIndex = 0;
            ui.menuAnimTimer = 0.0f;
            LOG_INFO("[Sys_UI] Splash → MainMenu (mouse click)");
        }
    }
}

// ============================================================
// RenderMainMenu
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

// 哪些菜单项在Phase 1中可用
static bool IsMenuItemEnabled(int index) {
    return index == 0 || index == 5 || index == 7; // START, SETTINGS, EXIT
}

void Sys_UI::RenderMainMenu(Registry& registry, float dt) {
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

    // ── 布局参数 ──
    float leftPanelW = vpSize.x * 0.35f;
    float rightPanelX = vpPos.x + leftPanelW;
    float rightPanelW = vpSize.x * 0.65f;

    // ── 右侧面板：战术背景动画 ──
    RenderMenuBackground(ui.globalTime, rightPanelX, vpPos.y, rightPanelW, vpSize.y);

    // 分隔线
    draw->AddLine(
        ImVec2(vpPos.x + leftPanelW, vpPos.y + 40.0f),
        ImVec2(vpPos.x + leftPanelW, vpPos.y + vpSize.y - 40.0f),
        IM_COL32(0, 100, 95, 80), 1.0f);

    // ── 左侧面板 ──
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

    // ── 菜单项 ──
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

    bool confirmed = false;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmed = true;
    }

    const Mouse* mouse = Window::GetMouse();

    for (int i = 0; i < kMenuItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.menuSelectedIndex);
        bool isEnabled  = IsMenuItemEnabled(i);

        // 选中项动画偏移
        float offsetX = isSelected ? 4.0f : 0.0f;
        float baseX = vpPos.x + panelPadX + offsetX;

        // 鼠标检测区域
        ImVec2 itemMin(vpPos.x + panelPadX - 5.0f, itemY - 2.0f);
        ImVec2 itemMax(vpPos.x + leftPanelW - 10.0f, itemY + menuItemH - 6.0f);

        bool mouseHover = false;
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            mouseHover = (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                          mousePos.y >= itemMin.y && mousePos.y <= itemMax.y);
        }

        if (mouseHover) {
            ui.menuSelectedIndex = static_cast<int8_t>(i);
            if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
                confirmed = true;
            }
        }

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
    if (confirmed && IsMenuItemEnabled(ui.menuSelectedIndex)) {
        switch (ui.menuSelectedIndex) {
            case 0: // START OPERATION
                ui.pendingSceneRequest = SceneRequest::StartGame;
                LOG_INFO("[Sys_UI] MainMenu → StartGame");
                break;
            case 5: // SETTINGS
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Settings;
                LOG_INFO("[Sys_UI] MainMenu → Settings");
                break;
            case 7: // EXIT
                ui.pendingSceneRequest = SceneRequest::QuitApp;
                LOG_INFO("[Sys_UI] MainMenu → QuitApp");
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

    // ESC 导航已在 OnUpdate 顶部集中处理，此处不再检测
}

// ============================================================
// RenderMenuBackground — 战术雷达动画
// ============================================================

void Sys_UI::RenderMenuBackground(float globalTime,
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

    // 扫描线尾迹（渐淡三角扇形）
    constexpr int trailSegments = 12;
    constexpr float trailAngle  = 0.5f; // ~30度拖尾
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

    // 数据节点（伪随机脉冲点）
    constexpr int nodeCount = 8;
    for (int i = 0; i < nodeCount; ++i) {
        // 用固定种子生成伪随机位置
        float seed = (float)(i * 137 + 42);
        float nx = panelX + panelW * (0.15f + 0.7f * (sinf(seed) * 0.5f + 0.5f));
        float ny = panelY + panelH * (0.15f + 0.7f * (cosf(seed * 1.7f) * 0.5f + 0.5f));

        float pulse = sinf(globalTime * 2.0f + seed) * 0.5f + 0.5f;
        uint8_t nodeAlpha = (uint8_t)(40.0f + pulse * 80.0f);
        float nodeR = 2.0f + pulse * 2.0f;

        draw->AddCircleFilled(ImVec2(nx, ny), nodeR,
            IM_COL32(0, 200, 190, nodeAlpha));

        // 连线到中心（微弱）
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
// RenderSettingsScreen
// ============================================================

void Sys_UI::RenderSettingsScreen(Registry& registry, float /*dt*/) {
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

    // 使用ImGui控件（样式已通过主题设定）
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
        LOG_INFO("[Sys_UI] Resolution selection: " << resolutions[resIdx]);
    }

    ImGui::Spacing();

    // 全屏切换（只写标志，由 Main.cpp 执行实际窗口操作）
    ImGui::Text("Fullscreen:");
    ImGui::SameLine(160.0f);
    if (ImGui::Checkbox("##Fullscreen", &ui.isFullscreen)) {
        ui.fullscreenChanged = true;
        LOG_INFO("[Sys_UI] Fullscreen request: " << (ui.isFullscreen ? "ON" : "OFF"));
    }

    ImGui::PopItemWidth();
    ImGui::Spacing();
    ImGui::Spacing();

    // BACK 按钮 — 返回来源画面（MainMenu 或 PauseMenu）
    ImGui::SetCursorScreenPos(ImVec2(contentX, panelY + panelH - 70.0f));
    if (ImGui::Button("< BACK", ImVec2(120, 35))) {
        UIScreen returnTo = (ui.previousScreen == UIScreen::PauseMenu)
                            ? UIScreen::PauseMenu : UIScreen::MainMenu;
        ui.activeScreen = returnTo;
        ui.previousScreen = UIScreen::Settings;
        LOG_INFO("[Sys_UI] Settings → " << (int)returnTo);
    }

    if (termFont) ImGui::PopFont();

    ImGui::End();

    // ESC 导航已在 OnUpdate 顶部集中处理，此处不再检测
}

// ============================================================
// RenderPauseMenu — 游戏内暂停菜单
// ============================================================

static const char* kPauseItems[] = {
    "RESUME",
    "SETTINGS",
    "RETURN TO MENU",
};
static constexpr int kPauseItemCount = 3;

void Sys_UI::RenderPauseMenu(Registry& registry, float /*dt*/) {
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

    bool confirmed = false;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmed = true;
    }

    const Mouse* mouse = Window::GetMouse();

    for (int i = 0; i < kPauseItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.pauseSelectedIndex);

        float baseX = panelX + 50.0f + (isSelected ? 4.0f : 0.0f);

        // 鼠标检测
        ImVec2 itemMin(panelX + 30.0f, itemY - 2.0f);
        ImVec2 itemMax(panelX + panelW - 30.0f, itemY + menuItemH - 6.0f);

        bool mouseHover = false;
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            mouseHover = (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                          mousePos.y >= itemMin.y && mousePos.y <= itemMax.y);
        }

        if (mouseHover) {
            ui.pauseSelectedIndex = static_cast<int8_t>(i);
            if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
                confirmed = true;
            }
        }

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
    if (confirmed) {
        switch (ui.pauseSelectedIndex) {
            case 0: // RESUME — 返回暂停前的游戏画面
                ui.activeScreen = ui.prePauseScreen;
                LOG_INFO("[Sys_UI] PauseMenu → Resume");
                break;
            case 1: // SETTINGS
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Settings;
                LOG_INFO("[Sys_UI] PauseMenu → Settings");
                break;
            case 2: // RETURN TO MENU
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[Sys_UI] PauseMenu → ReturnToMenu");
                break;
            default:
                break;
        }
    }

    // ESC 导航已在 OnUpdate 顶部集中处理，此处不再检测

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

// ============================================================
// RenderHUD — 游戏内 HUD（策划文档 §2.4）
// ============================================================
//
// 布局：
//   左上: 任务面板（行动名 + 当前目标）
//   右上: 警戒度仪表（数值条 + 状态文字）
//   上方中央: 倒计时（仅 alertLevel >= 1 时显示）
//   左下: 玩家状态（站立/下蹲/奔跑 + 伪装状态）
//   右下: 道具/武器快捷栏
//   全局: UI退化效果（与警戒度比率联动）

void Sys_UI::RenderHUD(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_GameplayState>()) return;
    auto& gs = registry.ctx<Res_GameplayState>();
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    const float pad = 20.0f;

    // ── 左上: 任务面板 ──
    RenderHUD_MissionPanel(draw, vpPos.x + pad, vpPos.y + pad,
                           gs.missionName, gs.objectiveText);

    // ── 右上: 警戒度仪表 ──
    const float alertW = 240.0f;
    const float alertH = 52.0f;
    RenderHUD_AlertGauge(draw,
        vpPos.x + vpSize.x - alertW - pad, vpPos.y + pad,
        alertW, alertH, gs.alertLevel, gs.alertMax);

    // ── 上方中央: 倒计时 ──
    RenderHUD_Countdown(draw,
        vpPos.x + vpSize.x * 0.5f, vpPos.y + pad,
        gs.countdownTimer, gs.countdownActive);

    // ── 左下: 玩家状态 ──
    RenderHUD_PlayerState(draw,
        vpPos.x + pad, vpPos.y + vpSize.y - 50.0f,
        static_cast<uint8_t>(gs.playerMoveState), gs.playerDisguised);

    // ── 右下: 道具/武器快捷栏 ──
    RenderHUD_ItemSlots(draw,
        vpPos.x + vpSize.x - 280.0f, vpPos.y + vpSize.y - 60.0f,
        gs);

    // ── UI 退化效果（策划文档 §2.4 UI退化表现）──
    float alertRatio = (gs.alertMax > 0.0f) ? gs.alertLevel / gs.alertMax : 0.0f;
    if (alertRatio > 0.2f) {
        RenderHUD_Degradation(draw, alertRatio, ui.globalTime,
                              vpSize.x, vpSize.y, vpPos.x, vpPos.y);
    }
}

// ============================================================
// RenderHUD_AlertGauge — 警戒度仪表（右上）
// ============================================================

void Sys_UI::RenderHUD_AlertGauge(ImDrawList* draw, float x, float y, float w, float h,
                                   float alertLevel, float alertMax) {
    AlertStatus status = GetAlertStatus(alertLevel);
    float ratio = (alertMax > 0.0f) ? alertLevel / alertMax : 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    // 背景面板
    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                        IM_COL32(8, 10, 16, 200), 3.0f);
    draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                  IM_COL32(0, 100, 95, 120), 3.0f, 0, 1.0f);

    // 标题
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(ImVec2(x + 8.0f, y + 4.0f),
                  IM_COL32(0, 140, 130, 180), "ALERT LEVEL");

    if (smallFont) ImGui::PopFont();

    // 进度条
    float barX = x + 8.0f;
    float barY = y + 20.0f;
    float barW = w - 16.0f;
    float barH = 10.0f;

    // 背景
    draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                        IM_COL32(20, 25, 30, 200), 2.0f);

    // 填充色根据状态变化
    ImU32 barColor;
    switch (status) {
        case AlertStatus::Safe:   barColor = IM_COL32(0, 200, 190, 220);  break;
        case AlertStatus::Search: barColor = IM_COL32(200, 200, 0, 220);  break;
        case AlertStatus::Alert:  barColor = IM_COL32(220, 140, 0, 220);  break;
        case AlertStatus::Hunt:   barColor = IM_COL32(220, 60, 20, 220);  break;
        case AlertStatus::Raid:   barColor = IM_COL32(255, 20, 20, 255);  break;
        default:                  barColor = IM_COL32(0, 200, 190, 220);  break;
    }

    float fillW = barW * ratio;
    if (fillW > 0.0f) {
        draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH),
                            barColor, 2.0f);
    }

    // 状态文字 + 数值
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    char statusBuf[48];
    snprintf(statusBuf, sizeof(statusBuf), "%s  %.0f / %.0f",
             GetAlertStatusText(status), alertLevel, alertMax);
    draw->AddText(ImVec2(barX, barY + barH + 3.0f), barColor, statusBuf);

    if (termFont) ImGui::PopFont();
}

// ============================================================
// RenderHUD_Countdown — 倒计时（上方中央）
// ============================================================

void Sys_UI::RenderHUD_Countdown(ImDrawList* draw, float cx, float y,
                                  float timer, bool active) {
    if (!active) return;

    // 倒计时永远清晰可读（不受退化影响）— 策划文档要求
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    int minutes = (int)(timer / 60.0f);
    int seconds = (int)(timer) % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);

    ImVec2 textSize = ImGui::CalcTextSize(buf);
    float textX = cx - textSize.x * 0.5f;

    // 发光底层
    draw->AddText(ImVec2(textX, y), IM_COL32(200, 20, 20, 60), buf);
    draw->AddText(ImVec2(textX - 1, y), IM_COL32(220, 40, 30, 120), buf);
    // 主层
    ImU32 timerColor = (timer < 30.0f) ? IM_COL32(255, 40, 30, 255)
                                       : IM_COL32(220, 180, 0, 255);
    draw->AddText(ImVec2(textX, y), timerColor, buf);

    if (titleFont) ImGui::PopFont();

    // 三角箭头装饰
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(ImVec2(textX - 25.0f, y + 5.0f),
                  IM_COL32(200, 60, 40, 150), ">>");
    draw->AddText(ImVec2(textX + textSize.x + 8.0f, y + 5.0f),
                  IM_COL32(200, 60, 40, 150), "<<");

    if (smallFont) ImGui::PopFont();
}

// ============================================================
// RenderHUD_MissionPanel — 任务面板（左上）
// ============================================================

void Sys_UI::RenderHUD_MissionPanel(ImDrawList* draw, float x, float y,
                                     const char* missionName, const char* objective) {
    float panelW = 300.0f;
    float panelH = 52.0f;

    // 半透明背景
    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + panelW, y + panelH),
                        IM_COL32(8, 10, 16, 180), 3.0f);
    draw->AddRect(ImVec2(x, y), ImVec2(x + panelW, y + panelH),
                  IM_COL32(0, 100, 95, 80), 3.0f, 0, 1.0f);

    // 行动名称
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(ImVec2(x + 8.0f, y + 4.0f),
                  IM_COL32(0, 140, 130, 180),
                  (missionName && missionName[0]) ? missionName : "OPERATION");

    if (smallFont) ImGui::PopFont();

    // 当前目标
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    char objBuf[64];
    snprintf(objBuf, sizeof(objBuf), "> %s",
             (objective && objective[0]) ? objective : "---");
    draw->AddText(ImVec2(x + 8.0f, y + 22.0f),
                  IM_COL32(0, 220, 210, 220), objBuf);

    if (termFont) ImGui::PopFont();
}

// ============================================================
// RenderHUD_PlayerState — 玩家状态（左下）
// ============================================================

void Sys_UI::RenderHUD_PlayerState(ImDrawList* draw, float x, float y,
                                    uint8_t moveState, bool disguised) {
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    // 移动状态
    const char* stateText;
    ImU32 stateColor;
    switch (static_cast<PlayerMoveState>(moveState)) {
        case PlayerMoveState::Crouching:
            stateText = "[CROUCH]";
            stateColor = IM_COL32(0, 180, 170, 200);
            break;
        case PlayerMoveState::Running:
            stateText = "[SPRINT]";
            stateColor = IM_COL32(220, 140, 0, 220);
            break;
        default:
            stateText = "[STAND]";
            stateColor = IM_COL32(100, 110, 120, 180);
            break;
    }
    draw->AddText(ImVec2(x, y), stateColor, stateText);

    // 伪装状态
    if (disguised) {
        ImVec2 stSize = ImGui::CalcTextSize(stateText);
        draw->AddText(ImVec2(x + stSize.x + 12.0f, y),
                      IM_COL32(100, 200, 100, 220), "[DISGUISED]");
    }

    if (termFont) ImGui::PopFont();
}

// ============================================================
// RenderHUD_ItemSlots — 道具/武器快捷栏（右下）
// ============================================================

void Sys_UI::RenderHUD_ItemSlots(ImDrawList* draw, float x, float y,
                                  const Res_GameplayState& gs) {
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    float slotW = 120.0f;
    float slotH = 24.0f;
    float gap   = 8.0f;

    // 道具槽标题
    draw->AddText(ImVec2(x, y - 16.0f), IM_COL32(0, 140, 130, 150), "ITEMS");
    draw->AddText(ImVec2(x + slotW + gap, y - 16.0f), IM_COL32(0, 140, 130, 150), "WEAPONS");

    for (int i = 0; i < 2; ++i) {
        // 道具槽
        float sx = x;
        float sy = y + i * (slotH + 4.0f);
        bool isActiveItem = (i == gs.activeItemSlot);

        ImU32 borderColor = isActiveItem ? IM_COL32(0, 220, 210, 200)
                                         : IM_COL32(0, 80, 75, 100);
        draw->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + slotW, sy + slotH),
                            IM_COL32(8, 10, 16, 180), 2.0f);
        draw->AddRect(ImVec2(sx, sy), ImVec2(sx + slotW, sy + slotH),
                      borderColor, 2.0f, 0, 1.0f);

        char itemBuf[32];
        if (gs.itemSlots[i].name[0]) {
            snprintf(itemBuf, sizeof(itemBuf), "%s x%d",
                     gs.itemSlots[i].name, gs.itemSlots[i].count);
        } else {
            snprintf(itemBuf, sizeof(itemBuf), "---");
        }
        ImU32 textColor = isActiveItem ? IM_COL32(0, 220, 210, 220)
                                       : IM_COL32(80, 90, 100, 180);
        draw->AddText(ImVec2(sx + 6.0f, sy + 4.0f), textColor, itemBuf);

        // 冷却条
        if (gs.itemSlots[i].cooldown > 0.0f) {
            float cdW = slotW * gs.itemSlots[i].cooldown;
            draw->AddRectFilled(ImVec2(sx, sy + slotH - 3.0f),
                                ImVec2(sx + cdW, sy + slotH),
                                IM_COL32(220, 60, 20, 150));
        }

        // 武器槽
        float wx = x + slotW + gap;
        bool isActiveWeapon = (i == gs.activeWeaponSlot);

        ImU32 wBorderColor = isActiveWeapon ? IM_COL32(0, 220, 210, 200)
                                            : IM_COL32(0, 80, 75, 100);
        draw->AddRectFilled(ImVec2(wx, sy), ImVec2(wx + slotW, sy + slotH),
                            IM_COL32(8, 10, 16, 180), 2.0f);
        draw->AddRect(ImVec2(wx, sy), ImVec2(wx + slotW, sy + slotH),
                      wBorderColor, 2.0f, 0, 1.0f);

        char wpnBuf[32];
        if (gs.weaponSlots[i].name[0]) {
            snprintf(wpnBuf, sizeof(wpnBuf), "%s x%d",
                     gs.weaponSlots[i].name, gs.weaponSlots[i].count);
        } else {
            snprintf(wpnBuf, sizeof(wpnBuf), "---");
        }
        ImU32 wTextColor = isActiveWeapon ? IM_COL32(0, 220, 210, 220)
                                          : IM_COL32(80, 90, 100, 180);
        draw->AddText(ImVec2(wx + 6.0f, sy + 4.0f), wTextColor, wpnBuf);
    }

    if (smallFont) ImGui::PopFont();
}

// ============================================================
// RenderHUD_Degradation — UI退化效果（策划文档 §2.4）
// ============================================================
//
// alertRatio 阈值：
//   0 ~ 0.2:  无效果
//   0.2 ~ 0.4: 轻微噪点
//   0.4 ~ 0.66: 噪点 + 文字闪烁模拟（缺角线条）
//   0.66 ~ 1.0: 上述 + HUD 元素抖动 + 扫描线加重
//   >= 1.0: 全面干扰（但倒计时始终清晰）

void Sys_UI::RenderHUD_Degradation(ImDrawList* draw, float alertRatio, float globalTime,
                                    float vpW, float vpH, float vpX, float vpY) {
    // ── 噪点层（alertRatio > 0.2）──
    if (alertRatio > 0.2f) {
        float noiseDensity = (alertRatio - 0.2f) * 60.0f; // 0~48 个噪点
        int dotCount = (int)noiseDensity;
        for (int i = 0; i < dotCount; ++i) {
            // 伪随机位置（使用 globalTime 作为种子变化）
            float seed = (float)i * 73.0f + globalTime * 37.0f;
            float nx = vpX + vpW * (0.05f + 0.9f * (sinf(seed) * 0.5f + 0.5f));
            float ny = vpY + vpH * (0.05f + 0.9f * (cosf(seed * 1.3f) * 0.5f + 0.5f));
            uint8_t alpha = (uint8_t)(20.0f + alertRatio * 40.0f);
            draw->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + 2.0f, ny + 2.0f),
                                IM_COL32(0, 200, 190, alpha));
        }
    }

    // ── 缺角线条（alertRatio > 0.4）──
    if (alertRatio > 0.4f) {
        float glitchIntensity = (alertRatio - 0.4f) * 3.0f; // 0~1.8
        int lineCount = (int)(glitchIntensity * 4.0f);
        for (int i = 0; i < lineCount; ++i) {
            float seed = (float)i * 131.0f + globalTime * 23.0f;
            float ly = vpY + vpH * (sinf(seed) * 0.5f + 0.5f);
            float lxStart = vpX + vpW * (cosf(seed * 0.7f) * 0.3f + 0.3f);
            float lxEnd = lxStart + 40.0f + 60.0f * sinf(seed * 1.7f);
            uint8_t alpha = (uint8_t)(15.0f + glitchIntensity * 25.0f);
            draw->AddLine(ImVec2(lxStart, ly), ImVec2(lxEnd, ly),
                          IM_COL32(0, 180, 170, alpha), 1.0f);
        }
    }

    // ── 扫描线加重（alertRatio > 0.66）──
    if (alertRatio > 0.66f) {
        float scanIntensity = (alertRatio - 0.66f) * 3.0f; // 0~1.0
        float scrollOffset = fmodf(globalTime * 30.0f, 4.0f);
        uint8_t scanAlpha = (uint8_t)(8.0f + scanIntensity * 20.0f);

        for (float y = vpY + scrollOffset; y < vpY + vpH; y += 4.0f) {
            draw->AddLine(ImVec2(vpX, y), ImVec2(vpX + vpW, y),
                          IM_COL32(0, 0, 0, scanAlpha), 1.0f);
        }
    }
}

// ============================================================
// RenderScanlineOverlay — CRT扫描线效果
// ============================================================

void Sys_UI::RenderScanlineOverlay(float globalTime) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 水平扫描线
    constexpr float lineSpacing = 3.0f;
    float scrollOffset = fmodf(globalTime * 15.0f, lineSpacing);
    ImU32 scanColor = IM_COL32(0, 0, 0, 18);

    for (float y = vpPos.y + scrollOffset; y < vpPos.y + vpSize.y; y += lineSpacing) {
        draw->AddLine(ImVec2(vpPos.x, y), ImVec2(vpPos.x + vpSize.x, y),
            scanColor, 1.0f);
    }

    // 微弱暗角效果（四角渐变）
    constexpr float vignetteSize = 200.0f;
    ImU32 vignetteColor = IM_COL32(0, 0, 0, 30);

    // 左上角
    draw->AddRectFilledMultiColor(
        vpPos,
        ImVec2(vpPos.x + vignetteSize, vpPos.y + vignetteSize),
        vignetteColor, IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    // 右上角
    draw->AddRectFilledMultiColor(
        ImVec2(vpPos.x + vpSize.x - vignetteSize, vpPos.y),
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vignetteSize),
        IM_COL32(0, 0, 0, 0), vignetteColor,
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
    // 左下角
    draw->AddRectFilledMultiColor(
        ImVec2(vpPos.x, vpPos.y + vpSize.y - vignetteSize),
        ImVec2(vpPos.x + vignetteSize, vpPos.y + vpSize.y),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        IM_COL32(0, 0, 0, 0), vignetteColor);
    // 右下角
    draw->AddRectFilledMultiColor(
        ImVec2(vpPos.x + vpSize.x - vignetteSize, vpPos.y + vpSize.y - vignetteSize),
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
        vignetteColor, IM_COL32(0, 0, 0, 0));
}

} // namespace ECS

#endif // USE_IMGUI
