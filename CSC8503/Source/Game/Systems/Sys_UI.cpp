#include "Sys_UI.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/C_D_Interactable.h"
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

    // ── DevMode debug hotkeys (F2-F9) ────────────────────
    if (ui.devMode && kb && registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();

        // F2: Cycle alertLevel (0/30/60/100/150)
        if (kb->KeyPressed(KeyCodes::F2)) {
            static const float kAlertCycle[] = { 0.0f, 30.0f, 60.0f, 100.0f, 150.0f };
            static int sAlertIdx = 0;
            sAlertIdx = (sAlertIdx + 1) % 5;
            gs.alertLevel = kAlertCycle[sAlertIdx];
            LOG_INFO("[DevMode] F2 alertLevel=" << gs.alertLevel);
        }

        // F3: Toggle countdownActive
        if (kb->KeyPressed(KeyCodes::F3)) {
            gs.countdownActive = !gs.countdownActive;
            if (gs.countdownActive) gs.countdownTimer = gs.countdownMax;
            LOG_INFO("[DevMode] F3 countdownActive=" << gs.countdownActive);
        }

        // F5: Preview GameOver (cycle reason 1/2/3)
        if (kb->KeyPressed(KeyCodes::F5)) {
            static uint8_t sReasonIdx = 0;
            sReasonIdx = (sReasonIdx % 3) + 1;  // 1, 2, 3
            gs.gameOverReason = sReasonIdx;
            gs.isGameOver = true;
            gs.gameOverTime = gs.playTime;
            ui.activeScreen = UIScreen::GameOver;
            ui.gameOverSelectedIndex = 0;
            LOG_INFO("[DevMode] F5 GameOver reason=" << (int)gs.gameOverReason);
        }

        // F6: Cycle noiseLevel (0/0.3/0.6/1.0)
        if (kb->KeyPressed(KeyCodes::F6)) {
            static const float kNoiseCycle[] = { 0.0f, 0.3f, 0.6f, 1.0f };
            static int sNoiseIdx = 0;
            sNoiseIdx = (sNoiseIdx + 1) % 4;
            gs.noiseLevel = kNoiseCycle[sNoiseIdx];
            LOG_INFO("[DevMode] F6 noiseLevel=" << gs.noiseLevel);
        }

        // F7: Trigger CRT transition
        if (kb->KeyPressed(KeyCodes::F7)) {
            ui.transitionActive   = true;
            ui.transitionTimer    = 0.0f;
            ui.transitionDuration = 0.5f;
            ui.transitionType     = (ui.transitionType == 0) ? 1 : 0;
            LOG_INFO("[DevMode] F7 transition type=" << (int)ui.transitionType);
        }

        // F8: Push test Toast (Info/Warning/Danger/Success cycle)
        if (kb->KeyPressed(KeyCodes::F8)) {
            static int sToastIdx = 0;
            const char* toastTexts[] = { "TEST INFO", "TEST WARNING", "TEST DANGER", "TEST SUCCESS" };
            ToastType toastTypes[] = { ToastType::Info, ToastType::Warning, ToastType::Danger, ToastType::Success };
            UI::PushToast(registry, toastTexts[sToastIdx], toastTypes[sToastIdx]);
            LOG_INFO("[DevMode] F8 Toast: " << toastTexts[sToastIdx]);
            sToastIdx = (sToastIdx + 1) % 4;
        }

        // F9: Toggle all C_D_Interactable.isEnabled
        if (kb->KeyPressed(KeyCodes::F9)) {
            static bool sInteractEnabled = true;
            sInteractEnabled = !sInteractEnabled;
            auto view = registry.view<C_D_Interactable>();
            view.each([&](EntityID, C_D_Interactable& inter) {
                inter.isEnabled = sInteractEnabled;
            });
            LOG_INFO("[DevMode] F9 Interactables enabled=" << sInteractEnabled);
        }
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

    // TAB key: hold mode for ItemWheel (only when in HUD)
    if (kb && ui.activeScreen == UIScreen::HUD) {
        bool tabDown = kb->KeyDown(KeyCodes::TAB);
        if (tabDown && !ui.itemWheelOpen) {
            // TAB pressed — open wheel
            ui.itemWheelOpen = true;
            ui.itemWheelWasOpen = true;
            ui.itemWheelSelected = -1;
        } else if (!tabDown && ui.itemWheelWasOpen) {
            // TAB released — confirm selection and close
            ui.itemWheelOpen = false;
            ui.itemWheelWasOpen = false;

            // Write selected slot to Res_GameState
            if (ui.itemWheelSelected >= 0 && registry.has_ctx<Res_GameState>()) {
                auto& gs = registry.ctx<Res_GameState>();
                switch (ui.itemWheelSelected) {
                    case 0: gs.activeItemSlot   = 0; break;
                    case 1: gs.activeItemSlot   = 1; break;
                    case 2: gs.activeWeaponSlot = 0; break;
                    case 3: gs.activeWeaponSlot = 1; break;
                    default: break;
                }
                LOG_INFO("[Sys_UI] ItemWheel confirmed: sector " << (int)ui.itemWheelSelected);
            }
            ui.itemWheelSelected = -1;
        }
    } else {
        ui.itemWheelOpen = false;
        ui.itemWheelWasOpen = false;
    }

    // Chat panel: auto open/close with HUD
    if (registry.has_ctx<Res_ChatState>()) {
        auto& chat = registry.ctx<Res_ChatState>();
        bool inHUD = (ui.activeScreen == UIScreen::HUD);
        if (inHUD && !chat.chatOpen) {
            chat.chatOpen = true;
        } else if (!inHUD && chat.chatOpen) {
            chat.chatOpen = false;
        }

        // Forward 1-4/W/S/Enter to chat reply navigation (HUD only)
        if (inHUD && kb && chat.replyCount > 0) {
            // Direct select with 1-4
            if (kb->KeyPressed(KeyCodes::NUM1) && chat.replyCount > 0) chat.selectedReply = 0;
            if (kb->KeyPressed(KeyCodes::NUM2) && chat.replyCount > 1) chat.selectedReply = 1;
            if (kb->KeyPressed(KeyCodes::NUM3) && chat.replyCount > 2) chat.selectedReply = 2;
            if (kb->KeyPressed(KeyCodes::NUM4) && chat.replyCount > 3) chat.selectedReply = 3;
        }
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
