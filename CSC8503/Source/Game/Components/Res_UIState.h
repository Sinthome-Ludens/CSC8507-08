#pragma once

#include <cstdint>

namespace ECS {

enum class UIScreen : uint8_t {
    None = 0,
    TitleScreen,
    Splash,
    MainMenu,
    Settings,
    PauseMenu,
    HUD,
    GameOver,
    Inventory,
    Loadout,
    Team,
};

enum class SceneRequest : uint8_t {
    None = 0,
    StartGame,
    ReturnToMenu,
    RestartLevel,
    QuitApp,
};

struct Res_UIState {
    UIScreen  activeScreen       = UIScreen::TitleScreen;
    UIScreen  previousScreen     = UIScreen::None;
    UIScreen  prePauseScreen     = UIScreen::None;
    bool      isUIBlockingInput  = true;

    SceneRequest pendingSceneRequest = SceneRequest::None;

    float     titleTimer         = 0.0f;
    float     splashTimer        = 0.0f;
    float     globalTime         = 0.0f;

    int8_t    menuSelectedIndex      = 0;
    int8_t    settingsSelectedIndex  = 0;
    int8_t    pauseSelectedIndex     = 0;
    int8_t    gameOverSelectedIndex  = 0;
    int8_t    resolutionIndex        = 0;

    bool      isFullscreen       = false;
    bool      fullscreenChanged  = false;
    bool      devMode            = false;

    float     masterVolume       = 0.8f;
    float     sfxVolume          = 0.8f;
    float     mouseSensitivity   = 0.5f;

    // Loadout / Inventory / Team
    int8_t    loadoutSelectedIndex  = 0;
    int8_t    inventorySelectedSlot = 0;
    float     teamStartTime        = 0.0f;

    // Item wheel
    bool      itemWheelOpen        = false;
    bool      itemWheelWasOpen     = false;   ///< 辅助检测 TAB 释放
    int8_t    itemWheelSelected    = -1;

    // Scene transition
    float     transitionTimer      = 0.0f;
    float     transitionDuration   = 0.5f;
    bool      transitionActive     = false;
    int8_t    transitionType       = 0;   // 0=FadeIn, 1=FadeOut
};

} // namespace ECS
