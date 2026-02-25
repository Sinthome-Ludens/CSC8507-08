#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 游戏UI系统 — 薄调度器
 *
 * 职责：
 *   - 管理全局UI时间和开发者模式切换
 *   - ESC导航（集中处理画面切换）
 *   - 按 activeScreen 调度到 ECS::UI 命名空间的无状态渲染函数
 *   - 扫描线叠加与输入阻塞标志
 *   - 开发者调试快捷键（F2/F3/F5）
 *
 * 渲染逻辑已提取至：
 *   - Game/UI/UI_Menus.h/.cpp   (Splash/MainMenu/Settings/PauseMenu)
 *   - Game/UI/UI_HUD.h/.cpp     (游戏内HUD)
 *   - Game/UI/UI_Effects.h/.cpp (CRT扫描线)
 *   - Game/UI/UI_GameOver.h/.cpp(GameOver画面)
 *
 * 状态数据存储在 Res_UIState / Res_GameplayState context 中，系统本身无状态。
 *
 * 执行优先级：500（所有渲染和Debug UI之后，确保游戏UI最后绘制）
 */
class Sys_UI : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;
};

} // namespace ECS

#endif // USE_IMGUI
