#pragma once

#include <cstdint>

namespace ECS {

enum class UIScreen : uint8_t {
    None = 0,        // 无UI覆盖（纯游戏画面）
    Splash,          // "按任意键开始"
    MainMenu,        // 主菜单
    Settings,        // 设置子画面
    PauseMenu,       // 游戏内暂停（未来）
    HUD,             // 游戏内HUD（未来）
    GameOver,        // 游戏结束（未来）
};

enum class SceneRequest : uint8_t {
    None = 0,
    StartGame,       // 切换到游戏场景
    ReturnToMenu,    // 返回主菜单
    QuitApp,         // 退出程序
};

struct Res_UIState {
    // 画面状态
    UIScreen  activeScreen       = UIScreen::Splash;
    UIScreen  previousScreen     = UIScreen::None;
    bool      isUIBlockingInput  = true;

    // 场景切换请求（由Sys_UI写入，Main.cpp读取并执行）
    SceneRequest pendingSceneRequest = SceneRequest::None;

    // Splash画面
    float     splashTimer        = 0.0f;
    bool      splashKeyPressed   = false;

    // 主菜单
    int8_t    menuSelectedIndex  = 0;
    float     menuAnimTimer      = 0.0f;

    // 过渡效果
    float     transitionAlpha    = 0.0f;
    bool      transitioningIn    = true;

    // 全局动画
    float     globalTime         = 0.0f;

    // 设置画面
    int8_t    resolutionIndex    = 0;        // 0=1280x720, 1=1920x1080

    // 暂停菜单
    int8_t    pauseSelectedIndex = 0;

    // 窗口状态（由Sys_UI写入，Main.cpp读取并执行）
    bool      isFullscreen       = false;
    bool      fullscreenChanged  = false;   // 本帧是否有全屏切换请求

    // 开发者模式（F1 切换）
    bool      devMode            = false;
};

} // namespace ECS
