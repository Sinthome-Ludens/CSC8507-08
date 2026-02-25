#include "Sys_UI.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Menus.h"
#include "Game/UI/UI_HUD.h"
#include "Game/UI/UI_Effects.h"
#include "Game/UI/UI_GameOver.h"
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

    // ── 开发者调试快捷键（仅 devMode 下）──
    if (ui.devMode && devKb && registry.has_ctx<Res_GameplayState>()) {
        auto& gs = registry.ctx<Res_GameplayState>();

        // F2: 循环 alertLevel（0→30→75→120→150→0）
        if (devKb->KeyPressed(KeyCodes::F2)) {
            if (gs.alertLevel < 1.0f)         gs.alertLevel = 30.0f;
            else if (gs.alertLevel < 31.0f)   gs.alertLevel = 75.0f;
            else if (gs.alertLevel < 76.0f)   gs.alertLevel = 120.0f;
            else if (gs.alertLevel < 121.0f)  gs.alertLevel = 150.0f;
            else                               gs.alertLevel = 0.0f;
            LOG_INFO("[Sys_UI] F2 alertLevel -> " << gs.alertLevel);
        }

        // F3: 切换 countdown 启动
        if (devKb->KeyPressed(KeyCodes::F3)) {
            gs.countdownActive = !gs.countdownActive;
            if (gs.countdownActive && gs.countdownTimer <= 0.0f) {
                gs.countdownTimer = gs.countdownMax;
            }
            LOG_INFO("[Sys_UI] F3 countdown: " << (gs.countdownActive ? "ON" : "OFF"));
        }

        // F5: 触发 GameOver（循环 reason 1/2/3）
        if (devKb->KeyPressed(KeyCodes::F5)) {
            gs.gameOverReason = (gs.gameOverReason % 3) + 1;
            gs.gameOverTime = ui.globalTime;
            ui.gameOverSelectedIndex = 0;
            ui.activeScreen = UIScreen::GameOver;
            LOG_INFO("[Sys_UI] F5 GameOver reason=" << (int)gs.gameOverReason);
        }
    }

    // ── ESC 导航（集中处理，防止同帧 KeyPressed 双重触发）──────────
    if (devKb && devKb->KeyPressed(KeyCodes::ESCAPE)) {
        switch (ui.activeScreen) {
            case UIScreen::None:
            case UIScreen::HUD:
                // 游戏场景 → 暂停菜单
                ui.prePauseScreen = ui.activeScreen;
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::PauseMenu;
                ui.pauseSelectedIndex = 0;
                LOG_INFO("[Sys_UI] Game -> PauseMenu");
                break;
            case UIScreen::PauseMenu:
                // 暂停菜单 → 恢复游戏
                ui.activeScreen = ui.prePauseScreen;
                LOG_INFO("[Sys_UI] PauseMenu -> Resume (ESC)");
                break;
            case UIScreen::Settings: {
                // 设置 → 返回来源画面
                UIScreen returnTo = (ui.previousScreen == UIScreen::PauseMenu)
                                    ? UIScreen::PauseMenu : UIScreen::MainMenu;
                ui.activeScreen = returnTo;
                ui.previousScreen = UIScreen::Settings;
                LOG_INFO("[Sys_UI] Settings -> " << (int)returnTo << " (ESC)");
                break;
            }
            case UIScreen::MainMenu:
                // 主菜单 → Splash
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Splash;
                ui.splashTimer = 0.0f;
                LOG_INFO("[Sys_UI] MainMenu -> Splash (ESC)");
                break;
            case UIScreen::GameOver:
                // GameOver 画面不响应 ESC
                break;
            default:
                break;
        }
    }

    // ── Dispatch 到对应画面渲染 ──
    switch (ui.activeScreen) {
        case UIScreen::Splash:    UI::RenderSplashScreen(registry, dt);    break;
        case UIScreen::MainMenu:  UI::RenderMainMenu(registry, dt);        break;
        case UIScreen::Settings:  UI::RenderSettingsScreen(registry, dt);  break;
        case UIScreen::PauseMenu: UI::RenderPauseMenu(registry, dt);      break;
        case UIScreen::HUD:       UI::RenderHUD(registry, dt);            break;
        case UIScreen::GameOver:  UI::RenderGameOverScreen(registry, dt); break;
        case UIScreen::None:
        default:
            break;
    }

    // 扫描线叠加（菜单类画面）
    if (ui.activeScreen != UIScreen::None && ui.activeScreen != UIScreen::HUD) {
        UI::RenderScanlineOverlay(ui.globalTime);
    }

    // 更新输入阻塞标志
    ui.isUIBlockingInput = (ui.activeScreen != UIScreen::None
                         && ui.activeScreen != UIScreen::HUD);
}

// ============================================================
// OnDestroy
// ============================================================

void Sys_UI::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_UI] OnDestroy.");
}

} // namespace ECS

#endif // USE_IMGUI
