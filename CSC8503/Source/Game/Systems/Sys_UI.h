#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 游戏UI系统 — 赛博朋克终端风格
 *
 * 职责：
 *   - 渲染Splash启动画面（"按任意键开始"）
 *   - 渲染主菜单（8项菜单 + 战术背景动画）
 *   - 渲染设置画面（分辨率/全屏切换）
 *   - 全局CRT扫描线叠加效果
 *   - 管理UI阻塞输入状态和鼠标可见性
 *
 * 状态数据存储在 Res_UIState context 中，系统本身无状态。
 *
 * 执行优先级：500（所有渲染和Debug UI之后，确保游戏UI最后绘制）
 */
class Sys_UI : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    void RenderSplashScreen   (Registry& registry, float dt);
    void RenderMainMenu       (Registry& registry, float dt);
    void RenderSettingsScreen (Registry& registry, float dt);
    void RenderPauseMenu      (Registry& registry, float dt);
    void RenderScanlineOverlay(float globalTime);

    // 主菜单辅助
    void RenderMenuBackground (float globalTime, float panelX, float panelY,
                               float panelW, float panelH);
};

} // namespace ECS

#endif // USE_IMGUI
