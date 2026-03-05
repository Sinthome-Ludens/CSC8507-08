#include "Sys_UI.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Menus.h"
#include "Game/UI/UI_Toast.h"
#include "Game/UI/UI_TitleScreen.h"
#include "Game/UI/UI_Effects.h"
#include "Game/UI/UI_HUD.h"
#include "Game/UI/UI_GameOver.h"
#include "Game/UI/UI_Team.h"
#include "Game/UI/UI_Loadout.h"
#include "Game/UI/UI_Inventory.h"
#include "Game/UI/UI_Chat.h"
#include "Game/UI/UI_ItemWheel.h"
#include "Game/UI/UI_Interaction.h"
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

    if (!registry.has_ctx<Res_ToastState>()) {
        registry.ctx_emplace<Res_ToastState>();
    }

    if (!registry.has_ctx<Res_InventoryState>()) {
        registry.ctx_emplace<Res_InventoryState>();
    }

    if (!registry.has_ctx<Res_ChatState>()) {
        registry.ctx_emplace<Res_ChatState>();
    }

    LOG_INFO("[Sys_UI] OnAwake — Fonts loaded, theme applied, all UI resources registered.");
}

// ============================================================
// OnUpdate
// ============================================================

void Sys_UI::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    ui.globalTime += dt;
    if (ui.globalTime > kGlobalTimeWrap) ui.globalTime -= kGlobalTimeWrap;

    // titleTimer 由 UI_TitleScreen::RenderTitleScreen 自行递增，此处不再重复

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
            case UIScreen::Inventory:
                ui.activeScreen = UIScreen::HUD;
                LOG_INFO("[Sys_UI] Inventory -> HUD (ESC)");
                break;
            case UIScreen::Loadout:
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::MainMenu;
                ui.menuSelectedIndex = 0;
                LOG_INFO("[Sys_UI] Loadout -> MainMenu (ESC)");
                break;
            case UIScreen::Team:
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::MainMenu;
                ui.menuSelectedIndex = 0;
                LOG_INFO("[Sys_UI] Team -> MainMenu (ESC)");
                break;
            case UIScreen::GameOver:
                // GameOver has its own menu; ESC does nothing
                break;
            default:
                break;
        }
    }

    // I key: toggle HUD <-> Inventory (only when in HUD or Inventory)
    if (kb && kb->KeyPressed(KeyCodes::I)) {
        if (ui.activeScreen == UIScreen::HUD) {
            ui.activeScreen = UIScreen::Inventory;
            ui.inventorySelectedSlot = 0;
            LOG_INFO("[Sys_UI] HUD -> Inventory (I)");
        } else if (ui.activeScreen == UIScreen::Inventory) {
            ui.activeScreen = UIScreen::HUD;
            LOG_INFO("[Sys_UI] Inventory -> HUD (I)");
        }
    }

    // TAB key: toggle ItemWheel (only when in HUD)
    if (kb && ui.activeScreen == UIScreen::HUD) {
        if (kb->KeyPressed(KeyCodes::TAB)) {
            ui.itemWheelOpen = !ui.itemWheelOpen;
            ui.itemWheelSelected = -1;
            LOG_INFO("[Sys_UI] ItemWheel: " << (ui.itemWheelOpen ? "OPEN" : "CLOSED"));
        }
    } else {
        ui.itemWheelOpen = false;
    }

    // Dispatch to render functions
    switch (ui.activeScreen) {
        case UIScreen::TitleScreen: UI::RenderTitleScreen(registry, dt);     break;
        case UIScreen::Splash:      UI::RenderSplashScreen(registry, dt);    break;
        case UIScreen::MainMenu:    UI::RenderMainMenu(registry, dt);        break;
        case UIScreen::Settings:    UI::RenderSettingsScreen(registry, dt);  break;
        case UIScreen::PauseMenu:   UI::RenderPauseMenu(registry, dt);      break;
        case UIScreen::HUD:         UI::RenderHUD(registry, dt);             break;
        case UIScreen::GameOver:    UI::RenderGameOverScreen(registry, dt);  break;
        case UIScreen::Inventory:   UI::RenderInventoryScreen(registry, dt); break;
        case UIScreen::Loadout:     UI::RenderLoadoutScreen(registry, dt);   break;
        case UIScreen::Team:        UI::RenderTeamScreen(registry, dt);      break;
        case UIScreen::None:
        default:
            break;
    }

    // HUD overlays (chat, interaction prompts, item wheel)
    if (ui.activeScreen == UIScreen::HUD) {
        UI::RenderChatPanel(registry, dt);
        UI::RenderInteractionPrompts(registry, dt);
        UI::RenderItemWheel(registry, dt);
    }

    // Update input blocking flag
    ui.isUIBlockingInput = (ui.activeScreen != UIScreen::None
                         && ui.activeScreen != UIScreen::HUD);

    // Scanline overlay (subtle CRT effect, always on)
    UI::RenderScanlineOverlay(ui.globalTime);

    // Scene transition overlay
    UI::RenderTransitionOverlay(registry, dt);

    // Toast 通知渲染（覆盖所有屏幕）
    UI::RenderToasts(registry, dt);
}

// ============================================================
// OnDestroy
// ============================================================

void Sys_UI::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_UI] OnDestroy.");
}

} // namespace ECS

#endif // USE_IMGUI
