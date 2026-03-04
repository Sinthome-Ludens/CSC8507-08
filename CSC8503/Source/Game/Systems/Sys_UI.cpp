#include "Sys_UI.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Menus.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS {

// globalTime 循环上限（≈ 2000π），防止 sinf/cosf 在大浮点值下精度退化
static constexpr float kGlobalTimeWrap = 6283.1853f;  // 2000 * PI

// ============================================================
// OnAwake
// ============================================================

void Sys_UI::OnAwake(Registry& registry) {
    UITheme::LoadFonts();
    UITheme::ApplyTheme();

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
    if (ui.globalTime > kGlobalTimeWrap) ui.globalTime -= kGlobalTimeWrap;

    if (ui.activeScreen == UIScreen::TitleScreen) {
        ui.titleTimer += dt;
    }

    // F1: toggle devMode
    const Keyboard* kb = Window::GetKeyboard();
    if (kb && kb->KeyPressed(KeyCodes::F1)) {
        ui.devMode = !ui.devMode;
        LOG_INFO("[Sys_UI] DevMode: " << (ui.devMode ? "ON" : "OFF"));
    }

    // ESC navigation
    if (kb && kb->KeyPressed(KeyCodes::ESCAPE)) {
        switch (ui.activeScreen) {
            case UIScreen::Settings:
                UI::NavigateBackFromSettings(ui);
                LOG_INFO("[Sys_UI] Settings -> back (ESC)");
                break;
            case UIScreen::MainMenu:
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::Splash;
                ui.splashTimer = 0.0f;
                LOG_INFO("[Sys_UI] MainMenu -> Splash (ESC)");
                break;
            case UIScreen::HUD:
                ui.prePauseScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::PauseMenu;
                ui.pauseSelectedIndex = 0;
                LOG_INFO("[Sys_UI] HUD -> PauseMenu (ESC)");
                break;
            case UIScreen::PauseMenu:
                ui.activeScreen = ui.prePauseScreen;
                LOG_INFO("[Sys_UI] PauseMenu -> Resume (ESC)");
                break;
            default:
                break;
        }
    }

    // Dispatch to render functions
    switch (ui.activeScreen) {
        case UIScreen::Splash:     UI::RenderSplashScreen(registry, dt);    break;
        case UIScreen::MainMenu:   UI::RenderMainMenu(registry, dt);        break;
        case UIScreen::Settings:   UI::RenderSettingsScreen(registry, dt);  break;
        case UIScreen::PauseMenu:  UI::RenderPauseMenu(registry, dt);      break;
        case UIScreen::TitleScreen: break;  // TODO: commit #2 — UI_TitleScreen
        case UIScreen::HUD:         break;  // TODO: commit #5 — UI_HUD
        case UIScreen::GameOver:    break;  // TODO: commit #7 — UI_GameOver
        case UIScreen::None:
        default:
            break;
    }

    // Update input blocking flag
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
