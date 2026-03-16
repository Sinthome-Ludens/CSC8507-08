/**
 * @file Sys_UI.cpp
 * @brief UI 状态机系统实现：菜单导航、光标仲裁、UI 资源注册与渲染分发。
 *
 * @details
 * - OnAwake：注册所有 UI ctx 资源（Res_UIState、Res_ToastState、Res_ActionNotifyState 等）
 * - OnUpdate：处理 F1 devMode、ESC 导航、I/TAB 热键，推进 playTime，
 *             按 activeScreen 分发到各 UI 渲染函数，最终仲裁光标状态
 * - OnDestroy：空操作（ctx 资源跨场景存活，由 Scene::OnExit 清理）
 */
#include "Sys_UI.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ActionNotifyState.h"
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
#include "Game/UI/UI_Loading.h"
#include "Game/UI/UI_Lobby.h"
#include "Game/UI/UI_MissionSelect.h"
#include "Game/UI/UI_ActionNotify.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/SaveManager.h"

using namespace NCL;

namespace ECS {

// globalTime 循环上限（≈ 2000π），防止 sinf/cosf 在大浮点值下精度退化
static constexpr float kGlobalTimeWrap = 6283.1853f;  // 2000 * PI

// ============================================================
// OnAwake
// ============================================================
/**
 * @brief 系统初始化：加载字体、应用主题，注册所有 UI ctx 资源（含 Res_ActionNotifyState）。
 * @param registry ECS 注册表
 */
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

    if (!registry.has_ctx<Res_LobbyState>()) {
        registry.ctx_emplace<Res_LobbyState>();
    }

    if (!registry.has_ctx<Res_ActionNotifyState>()) {
        registry.ctx_emplace<Res_ActionNotifyState>();
    }

    // 加载存档到 UIState 缓存（菜单阶段只有 UIState 存在）
    if (ECS::HasSaveFile()) {
        ECS::LoadGame(registry);
    }

    LOG_INFO("[Sys_UI] OnAwake — Fonts loaded, theme applied, all UI resources registered.");
}

// ============================================================
// OnUpdate
// ============================================================
/**
 * @brief 每帧处理热键输入（F1/ESC/I/TAB）、推进 playTime，并按 activeScreen
 *        分发到对应 UI 渲染函数，最终仲裁光标状态与 CRT 特效。
 * @param registry ECS 注册表
 * @param dt       帧时间（秒）
 */
void Sys_UI::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    ui.globalTime += dt;
    if (ui.globalTime > kGlobalTimeWrap) ui.globalTime -= kGlobalTimeWrap;

    // titleTimer 由 UI_TitleScreen::RenderTitleScreen 自行递增，此处不再重复

    // F1: toggle devMode
    const auto& input = registry.ctx<Res_Input>();
    if (input.keyPressed[KeyCodes::F1]) {
        ui.devMode = !ui.devMode;
        LOG_INFO("[Sys_UI] DevMode: " << (ui.devMode ? "ON" : "OFF"));
    }

    // ── DevMode debug hotkeys (F2-F9) ────────────────────
    if (ui.devMode && registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();

        // F2: Cycle alertLevel (0/25/50/75/100) — derive next from current value
        if (input.keyPressed[KeyCodes::F2]) {
            static const float kAlertCycle[] = { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f };
            float next = kAlertCycle[0];
            for (int i = 0; i < 5; ++i) {
                if (kAlertCycle[i] > gs.alertLevel + 0.01f) { next = kAlertCycle[i]; break; }
            }
            gs.alertLevel = next;
            LOG_INFO("[DevMode] F2 alertLevel=" << gs.alertLevel);
        }

        // F3: Toggle countdownActive
        if (input.keyPressed[KeyCodes::F3]) {
            gs.countdownActive = !gs.countdownActive;
            if (gs.countdownActive) gs.countdownTimer = gs.countdownMax;
            LOG_INFO("[DevMode] F3 countdownActive=" << gs.countdownActive);
        }

        // F5: Preview GameOver (cycle reason 1/2/3) — derive from current
        if (input.keyPressed[KeyCodes::F5]) {
            gs.gameOverReason = (gs.gameOverReason % 3) + 1;
            gs.isGameOver = true;
            gs.gameOverTime = gs.playTime;
            ui.activeScreen = UIScreen::GameOver;
            ui.gameOverSelectedIndex = 0;
            LOG_INFO("[DevMode] F5 GameOver reason=" << (int)gs.gameOverReason);
        }

        // F6: Cycle noiseLevel (0/0.3/0.6/1.0) — derive from current value
        if (input.keyPressed[KeyCodes::F6]) {
            static const float kNoiseCycle[] = { 0.0f, 0.3f, 0.6f, 1.0f };
            float next = kNoiseCycle[0];
            for (int i = 0; i < 4; ++i) {
                if (kNoiseCycle[i] > gs.noiseLevel + 0.01f) { next = kNoiseCycle[i]; break; }
            }
            gs.noiseLevel = next;
            LOG_INFO("[DevMode] F6 noiseLevel=" << gs.noiseLevel);
        }

        // F7: Trigger CRT transition
        if (input.keyPressed[KeyCodes::F7]) {
            ui.transitionActive   = true;
            ui.transitionTimer    = 0.0f;
            ui.transitionDuration = 0.5f;
            ui.transitionType     = (ui.transitionType == 0) ? 1 : 0;
            LOG_INFO("[DevMode] F7 transition type=" << (int)ui.transitionType);
        }

        // F8: Push test Toast (Info/Warning/Danger/Success cycle)
        if (input.keyPressed[KeyCodes::F8]) {
            const char* toastTexts[] = { "TEST INFO", "TEST WARNING", "TEST DANGER", "TEST SUCCESS" };
            ToastType toastTypes[] = { ToastType::Info, ToastType::Warning, ToastType::Danger, ToastType::Success };
            UI::PushToast(registry, toastTexts[ui.devToastCycle], toastTypes[ui.devToastCycle]);
            LOG_INFO("[DevMode] F8 Toast: " << toastTexts[ui.devToastCycle]);
            ui.devToastCycle = (ui.devToastCycle + 1) % 4;
        }

        // F9: Toggle all C_D_Interactable.isEnabled — read first entity's value, then invert
        if (input.keyPressed[KeyCodes::F9]) {
            bool newVal = true;
            auto view = registry.view<C_D_Interactable>();
            bool first = true;
            view.each([&](EntityID, C_D_Interactable& inter) {
                if (first) { newVal = !inter.isEnabled; first = false; }
                inter.isEnabled = newVal;
            });
            LOG_INFO("[DevMode] F9 Interactables enabled=" << newVal);
        }
    }

    // ESC navigation
    if (input.keyPressed[KeyCodes::ESCAPE]) {
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
            case UIScreen::HUD: {
                bool isMP = registry.has_ctx<Res_GameState>()
                         && registry.ctx<Res_GameState>().isMultiplayer;
                if (isMP) {
                    UI::PushToast(registry, "PAUSE UNAVAILABLE IN MULTIPLAYER", ToastType::Warning);
                    LOG_INFO("[Sys_UI] HUD ESC blocked — multiplayer mode");
                } else {
                    ui.prePauseScreen = ui.activeScreen;
                    ui.activeScreen = UIScreen::PauseMenu;
                    ui.pauseSelectedIndex = 0;
                    LOG_INFO("[Sys_UI] HUD -> PauseMenu (ESC)");
                }
                break;
            }
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
            case UIScreen::MissionSelect:
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::MainMenu;
                ui.menuSelectedIndex = 0;
                LOG_INFO("[Sys_UI] MissionSelect -> MainMenu (ESC)");
                break;
            case UIScreen::Team:
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::MainMenu;
                ui.menuSelectedIndex = 0;
                LOG_INFO("[Sys_UI] Team -> MainMenu (ESC)");
                break;
            case UIScreen::Lobby:
                ui.previousScreen = ui.activeScreen;
                ui.activeScreen = UIScreen::MainMenu;
                ui.menuSelectedIndex = 0;
                LOG_INFO("[Sys_UI] Lobby -> MainMenu (ESC)");
                break;
            case UIScreen::GameOver:
                // GameOver has its own menu; ESC does nothing
                break;
            default:
                break;
        }
    }

    // I key: toggle HUD <-> Inventory (only when in HUD or Inventory)
    if (input.keyPressed[KeyCodes::I]) {
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
    if (ui.activeScreen == UIScreen::HUD) {
        bool tabDown = input.keyStates[KeyCodes::TAB];
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

        // 1-4 key handling removed (Fix 7): Sys_Chat is the single owner of reply input
    }

    // HUD 状态下累加 playTime（写操作在 System 层，UI 只读）
    if (ui.activeScreen == UIScreen::HUD && registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();
        gs.playTime += dt;
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
        case UIScreen::Loadout:        UI::RenderLoadoutScreen(registry, dt);   break;
        case UIScreen::MissionSelect: UI::RenderMissionSelect(registry, dt);  break;
        case UIScreen::Team:        UI::RenderTeamScreen(registry, dt);      break;
        case UIScreen::Loading:     UI::RenderLoadingScreen(registry, dt);   break;
        case UIScreen::Lobby:       UI::RenderLobbyScreen(registry, dt);     break;
        case UIScreen::None:
        default:
            break;
    }

    // HUD overlays (chat, interaction prompts, item wheel, action notify)
    if (ui.activeScreen == UIScreen::HUD) {
        UI::RenderChatPanel(registry, dt);
        UI::RenderInteractionPrompts(registry, dt);
        UI::RenderItemWheel(registry, dt);
        UI::RenderActionNotify(registry, dt);
    }

    // Update input blocking flag
    ui.isUIBlockingInput = (ui.activeScreen != UIScreen::None
                         && ui.activeScreen != UIScreen::HUD);

    // Sys_UI (priority 500) runs AFTER Sys_Camera (priority 50/180).
    // 最终仲裁光标状态：菜单模式显示光标，游戏模式依据 gameCursorFree（Alt 键）。
    if (ui.isUIBlockingInput) {
        ui.cursorVisible = true;
        ui.cursorLocked  = false;
    } else if (ui.gameCursorFree || ui.itemWheelOpen) {
        ui.cursorVisible = true;
        ui.cursorLocked  = false;
    } else {
        ui.cursorVisible = false;
        ui.cursorLocked  = true;
    }

    // CRT effects (menus only — skipped during HUD/None)
    if (ui.activeScreen != UIScreen::HUD && ui.activeScreen != UIScreen::None) {
        UI::RenderScanlineOverlay(ui.globalTime);
        UI::RenderVignetteOverlay();
    }

    // Trigger CRT FadeOut transition on pending scene requests
    if (ui.pendingSceneRequest != SceneRequest::None && !ui.transitionActive) {
        ui.transitionSceneRequest = ui.pendingSceneRequest;   // 暂存到过渡期
        ui.pendingSceneRequest    = SceneRequest::None;       // 清零防重触发
        ui.transitionActive       = true;
        ui.transitionTimer        = 0.0f;
        ui.transitionDuration     = 0.5f;
        ui.transitionType         = 1;  // FadeOut (CRT shrink)
    }

    // Scene transition overlay
    UI::RenderTransitionOverlay(registry, dt);

    // FadeOut 结束后：进入 Loading 画面，等待最小时长后再交还场景请求
    if (!ui.transitionActive && ui.transitionSceneRequest != SceneRequest::None
        && ui.activeScreen != UIScreen::Loading) {
        // FadeOut 刚完成 → 切换到 Loading 画面
        ui.activeScreen    = UIScreen::Loading;
        ui.loadingTimer    = 0.0f;
        ui.loadingMsgIndex = 0;
        ui.loadingMsgTimer = 0.0f;
        LOG_INFO("[Sys_UI] FadeOut done -- entering Loading screen");
    }

    // Loading 画面计时完成 → 交还场景请求给 Main.cpp
    if (ui.activeScreen == UIScreen::Loading
    && ui.loadingTimer >= ui.loadingMinDuration
    && ui.transitionSceneRequest != SceneRequest::None
    && !ui.sceneRequestDispatched) {
/*
        LOG_INFO("[Sys_UI] DEBUG activeScreen="
            + std::to_string((int)ui.activeScreen)
            + " loadingTimer=" + std::to_string(ui.loadingTimer)
            + " transitionReq=" + std::to_string((int)ui.transitionSceneRequest));
*/

        ui.pendingSceneRequest    = ui.transitionSceneRequest;
        ui.transitionSceneRequest = SceneRequest::None;
        ui.activeScreen           = UIScreen::None;
        ui.loadingTimer           = 0.0f;
        ui.sceneRequestDispatched = true;  // ← 锁住，防止重入
        LOG_INFO("[Sys_UI] Loading complete -- handing SceneRequest to Main.cpp");
    }

    // Toast 通知渲染（覆盖所有屏幕）
    UI::RenderToasts(registry, dt);
}

// ============================================================
// OnDestroy
// ============================================================
/**
 * @brief 系统销毁（空操作）。ctx 资源由 Scene::OnExit 负责清理。
 * @param registry ECS 注册表（未使用）
 */
void Sys_UI::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_UI] OnDestroy.");
}

} // namespace ECS

#endif // USE_IMGUI
