/**
 * @file Res_UIState.h
 * @brief UI 主状态资源：画面导航、分辨率/全屏设置、光标管理、场景过渡
 *
 * @details
 * Session 级 ctx 资源，跨场景保持用户设置（分辨率/灵敏度/全屏等）。
 * 永不在 Scene::OnExit 中清除。
 *
 * @see Sys_UI（读写此资源，驱动 UI 状态机）
 */
#pragma once

#include <cstdint>

namespace ECS {

struct ResolutionPreset {
    int width;
    int height;
    const char* label;
};
inline constexpr ResolutionPreset kResolutions[] = {
    { 1280, 720,  "1280 x 720"  },
    { 1600, 900,  "1600 x 900"  },
    { 1920, 1080, "1920 x 1080" },
};
inline constexpr int kResolutionCount = 3;

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
    MissionSelect,
    Team,
    Loading,
    Lobby,
};

enum class SceneRequest : uint8_t {
    None = 0,
    StartGame,
    ReturnToMenu,
    RestartLevel,
    QuitApp,
    HostGame,
    JoinGame,
    NextLevel,          ///< 关卡序列：前进到下一张地图
};

struct Res_UIState {
    UIScreen  activeScreen       = UIScreen::TitleScreen;
    UIScreen  previousScreen     = UIScreen::None;
    UIScreen  prePauseScreen     = UIScreen::None;
    bool      isUIBlockingInput  = false;

    SceneRequest pendingSceneRequest = SceneRequest::None;

    float     titleTimer         = 0.0f;
    float     splashTimer        = 0.0f;
    float     globalTime         = 0.0f;

    int8_t    menuSelectedIndex      = 0;
    int8_t    settingsSelectedIndex  = 0;
    int8_t    pauseSelectedIndex     = 0;
    int8_t    gameOverSelectedIndex  = 0;
    int8_t    resolutionIndex        = 2;   // default 1920x1080 (0=720p, 1=900p, 2=1080p)
    bool      resolutionChanged      = false;

    bool      isFullscreen       = false;
    bool      fullscreenChanged  = false;
    bool      devMode            = false;
    bool      sceneRequestDispatched = false;

    float     masterVolume       = 0.8f;
    float     sfxVolume          = 0.8f;
    float     mouseSensitivity   = 0.5f;

    // Loadout / Inventory / Team
    int8_t    loadoutSelectedIndex  = 0;
    int8_t    inventorySelectedSlot = 0;
    float     teamStartTime        = 0.0f;

    // Mission Select
    int8_t    missionSelectedMap        = 0;      ///< 选中的关卡索引
    int8_t    missionSelectedTab        = 0;      ///< 0=关卡, 1=道具, 2=武器
    int8_t    missionCursorPerTab[3]    = {};     ///< 每个 tab 内的光标位置
    int8_t    missionEquippedItems[2]   = { -1, -1 };
    int8_t    missionEquippedWeapons[2] = { -1, -1 };

    // Item wheel
    bool      itemWheelOpen        = false;
    bool      itemWheelWasOpen     = false;   ///< 辅助检测 TAB 释放
    int8_t    itemWheelSelected    = -1;

    // Scene transition
    float        transitionTimer         = 0.0f;
    float        transitionDuration      = 0.5f;
    bool         transitionActive        = false;
    int8_t       transitionType          = 0;   // 0=FadeIn, 1=FadeOut
    SceneRequest transitionSceneRequest  = SceneRequest::None;  // 过渡期间暂存的场景请求

    // Lobby
    int8_t    lobbySelectedIndex   = 0;     // 0=HOST, 1=JOIN, 2=BACK

    // Loading screen
    float     loadingTimer         = 0.0f;
    float     loadingMinDuration   = 1.5f;  // 最少显示时长（秒）
    uint8_t   loadingMsgIndex      = 0;     // 当前显示到的系统消息索引
    float     loadingMsgTimer      = 0.0f;  // 消息轮播计时器

    // Cursor management
    // gameCursorFree: Sys_Camera 写入（Alt 键状态），Sys_UI 读取用于 HUD 模式光标决策
    // cursorVisible / cursorLocked: Sys_Camera 写 fallback，Sys_UI 写最终值，Main.cpp 读取并应用
    bool      gameCursorFree       = false;  ///< true = Alt 按住，游戏内光标自由模式
    bool      cursorVisible        = true;   ///< Main.cpp → ShowOSPointer()
    bool      cursorLocked         = false;  ///< Main.cpp → LockMouseToWindow()

    // DevMode (Fix 3: toast cycle needs memory; others derive from current state)
    uint8_t   devToastCycle        = 0;

    // Loadout temporary state (Fix 6: moved from file-scope statics)
    int8_t    loadoutEquippedItems[2]   = { -1, -1 };
    int8_t    loadoutEquippedWeapons[2] = { -1, -1 };
    bool      loadoutInitialized        = false;

    // Saved inventory storeCount cache (populated by SaveManager::LoadGame)
    uint8_t   savedStoreCount[5]       = {};
    bool      hasSavedInventory        = false;

    // ── Map sequence (campaign: random 5-pick-3, sorted by map ID) ──
    // Map IDs: 0=HangerA, 1=HangerB, 2=Helipad, 3=Lab, 4=Dock
    static constexpr int MAP_SEQUENCE_LENGTH = 3;
    uint8_t   mapSequence[MAP_SEQUENCE_LENGTH] = {0, 1, 2};  ///< 关卡地图 ID
    uint8_t   mapSequenceIndex         = 0;           ///< 当前关卡在序列中的位置
    bool      mapSequenceGenerated     = false;       ///< 是否已生成过序列
};

} // namespace ECS
