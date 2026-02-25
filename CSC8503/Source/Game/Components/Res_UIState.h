#pragma once

#include <cstdint>

namespace ECS {

enum class UIScreen : uint8_t {
    None = 0,        // 无UI覆盖（纯游戏画面）
    TitleScreen,     // 启动标题画面（CD机风格）
    Splash,          // "按任意键开始"
    MainMenu,        // 主菜单
    Settings,        // 设置子画面
    PauseMenu,       // 游戏内暂停菜单
    HUD,             // 游戏内HUD
    GameOver,        // 游戏结束
    Inventory,       // 背包界面
};

enum class SceneRequest : uint8_t {
    None = 0,
    StartGame,       // 切换到游戏场景
    ReturnToMenu,    // 返回主菜单
    QuitApp,         // 退出程序
    RestartLevel,    // 重启当前关卡
};

struct Res_UIState {
    // 画面状态
    UIScreen  activeScreen       = UIScreen::TitleScreen;
    UIScreen  previousScreen     = UIScreen::None;
    UIScreen  prePauseScreen     = UIScreen::None;   // 进入PauseMenu前的游戏画面（HUD/None）
    bool      isUIBlockingInput  = true;

    // 场景切换请求（由Sys_UI写入，Main.cpp读取并执行）
    SceneRequest pendingSceneRequest = SceneRequest::None;

    // 标题画面
    float     titleTimer         = 0.0f;

    // Splash画面
    float     splashTimer        = 0.0f;

    // 主菜单
    int8_t    menuSelectedIndex  = 0;

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

    // GameOver画面
    int8_t    gameOverSelectedIndex = 0;  // 0=RETRY, 1=RETURN TO MENU

    // 场景过渡效果
    float     transitionTimer    = 0.0f;
    float     transitionDuration = 0.8f;
    bool      transitionActive   = false;
    uint8_t   transitionType     = 0;    // 0=fade-in, 1=fade-out

    // 道具快捷轮盘
    bool      itemWheelOpen      = false;
    int8_t    itemWheelSelected  = -1;   // -1=无选中

    // 背包界面
    int8_t    inventorySelectedSlot = 0;
};

} // namespace ECS
