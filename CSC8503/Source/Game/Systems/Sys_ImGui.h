#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief ImGui 渲染系统
 *
 * 职责：
 *   - 渲染全局菜单栏（Windows / Rendering / Test Scene 子菜单）
 *   - 渲染性能 Debug 窗口（FPS / Frame Time / Entity Count）
 *   - 渲染 NCL Status 窗口（GameWorld 对象数 / Physics 状态）
 *   - 渲染统一 Rendering 面板（所有渲染参数，CollapsingHeader 分组）
 *   - 渲染测试场景控制面板（Spawn Cube / Delete Last / Gravity 开关）
 *   - 渲染 Cube Debug 浮窗（per-cube 位置 / 重力 / Body 状态）
 *
 * ## Debug 菜单规范
 *
 * 所有可调渲染参数必须集中在 "Rendering" 面板中，
 * 用 ImGui::CollapsingHeader 按功能分组（如 "Bloom"、"Tone Mapping"、"SSAO" 等）。
 * 禁止为单个渲染功能创建独立浮窗。新增 Phase 时在此面板追加对应 Section。
 *
 * 执行优先级：300（Render=200 之后，确保渲染桥接已完成）
 */
class Sys_ImGui : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    // ── 核心窗口 ──────────────────────────────────────────────────────────
    void RenderMainMenuBar  (Registry& registry);
    void RenderDebugWindow  (Registry& registry, float dt);
    void RenderNCLStatus    (Registry& registry);

    // ── 统一 Rendering 面板（所有渲染参数集中于此）──────────────────────
    void RenderRenderingPanel(Registry& registry);
    void RenderSection_PostProcess(Registry& registry);  ///< Bloom / Tone Mapping

    // ── Test Scene 调试控制（状态读写 Res_TestState context）────────────
    void RenderTestControlsWindow(Registry& registry);
    void RenderCubeDebugWindow   (Registry& registry);

    void SpawnCube     (Registry& registry);
    void DeleteLastCube(Registry& registry);
    void SetGravityAll (Registry& registry, float factor);

    // ── 窗口可见标志 ─────────────────────────────────────────────────────
    bool m_ShowDemoWindow     = false;
    bool m_ShowDebugWindow    = true;
    bool m_ShowNCLStatus      = true;
    bool m_ShowRenderingPanel = true;
};

} // namespace ECS
#endif
