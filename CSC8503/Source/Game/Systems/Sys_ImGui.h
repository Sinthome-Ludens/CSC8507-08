#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief ImGui 渲染系统
 *
 * 职责：
 *   - 渲染全局菜单栏（Windows / Test Scene 子菜单）
 *   - 渲染性能 Debug 窗口（FPS / Frame Time / Entity Count）
 *   - 渲染 NCL Status 窗口（GameWorld 对象数 / Physics 状态）
 *   - 渲染测试场景控制面板（Spawn Cube/Capsule / Delete / Gravity 开关）
 *   - 渲染 Cube Debug 浮窗（per-cube 位置 / 重力 / Body 状态）
 *
 * 测试场景状态（cube/capsule 实体列表、spawn 索引）存储在 Res_TestState context 中，
 * 系统本身保持无状态（除必要的窗口可见标志外）。
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

    // ── Test Scene 调试控制（状态读写 Res_TestState context）────────────
    void RenderTestControlsWindow(Registry& registry);  ///< 控制面板：Cube/Capsule Spawn/Delete/Gravity
    void RenderCubeDebugWindow   (Registry& registry);  ///< 浮动 Debug 窗口：per-cube 状态
    void RenderNetworkDebugWindow(Registry& registry);  ///< 网络调试面板：状态/流量/NetID映射

    void SpawnCube     (Registry& registry);             ///< 通过 PrefabFactory 生成动态方块
    void DeleteLastCube(Registry& registry);             ///< 销毁最后生成的方块
    void SetGravityAll (Registry& registry, float factor); ///< 批量修改 gravity_factor

    // ── 窗口可见标志（系统配置，非游戏状态）────────────────────────────
    bool m_ShowDemoWindow  = false;
    bool m_ShowDebugWindow = true;
    bool m_ShowNCLStatus   = true;
    bool m_WireframeMode   = false;
};

} // namespace ECS
#endif
