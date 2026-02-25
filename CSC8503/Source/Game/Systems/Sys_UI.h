#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

struct ImDrawList;   // imgui forward declaration（避免在头文件 include imgui.h）

namespace ECS {

/**
 * @brief 游戏UI系统 — 赛博朋克终端风格
 *
 * 职责：
 *   - 渲染Splash启动画面（"按任意键开始"）
 *   - 渲染主菜单（8项菜单 + 战术背景动画）
 *   - 渲染设置画面（分辨率/全屏切换）
 *   - 渲染游戏内HUD（警戒度/倒计时/任务/道具/玩家状态）
 *   - 全局CRT扫描线叠加效果
 *   - UI退化效果（与警戒度联动）
 *   - 管理UI阻塞输入状态和鼠标可见性
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

private:
    // ── 菜单画面 ──
    void RenderSplashScreen   (Registry& registry, float dt);
    void RenderMainMenu       (Registry& registry, float dt);
    void RenderSettingsScreen (Registry& registry, float dt);
    void RenderPauseMenu      (Registry& registry, float dt);
    void RenderScanlineOverlay(float globalTime);

    // ── 游戏内 HUD（策划文档 §2.4）──
    void RenderHUD             (Registry& registry, float dt);
    void RenderHUD_AlertGauge  (ImDrawList* draw, float x, float y, float w, float h,
                                float alertLevel, float alertMax);
    void RenderHUD_Countdown   (ImDrawList* draw, float cx, float y,
                                float timer, bool active);
    void RenderHUD_MissionPanel(ImDrawList* draw, float x, float y,
                                const char* missionName, const char* objective);
    void RenderHUD_PlayerState (ImDrawList* draw, float x, float y,
                                uint8_t moveState, bool disguised);
    void RenderHUD_ItemSlots   (ImDrawList* draw, float x, float y,
                                const struct Res_GameplayState& gs);
    void RenderHUD_Degradation (ImDrawList* draw, float alertRatio, float globalTime,
                                float vpW, float vpH, float vpX, float vpY);

    // ── 主菜单辅助 ──
    void RenderMenuBackground (float globalTime, float panelX, float panelY,
                               float panelW, float panelH);
};

} // namespace ECS

#endif // USE_IMGUI
