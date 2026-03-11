/**
 * @file Sys_ImGui.h
 * @brief ImGui 渲染系统接口声明：主菜单栏、Debug 窗口、测试场景控制面板
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"
#include "Game/Components/Res_TestState.h"

namespace ECS {

/**
 * @brief ImGui 渲染系统
 *
 * 职责：
 *   - 渲染全局菜单栏（Windows / DebugSceneSelector 子菜单）
 *   - 渲染性能 Debug 窗口（FPS / Frame Time / Entity Count）
 *   - 渲染 NCL Status 窗口（GameWorld 对象数 / Physics 状态）
 *   - 渲染测试场景控制面板（Spawn Cube/Capsule / Delete / Gravity 开关）
 *
 * 测试场景状态（cube/capsule 实体列表、spawn 索引）存储在 Res_TestState context 中，
 * 系统本身保持无状态（除必要的窗口可见标志外）。
 *
 * 执行优先级：300（Render=200 之后，确保渲染桥接已完成）
 */
class Sys_ImGui : public ISystem {
public:
    /**
     * @brief 初始化 ImGui 通用窗口系统。
     * @param registry 当前场景注册表
     */
    void OnAwake  (Registry& registry)           override;

    /**
     * @brief 渲染 ImGui 通用菜单栏与基础调试窗口。
     * @param registry 当前场景注册表
     * @param dt 本帧时间步长
     */
    void OnUpdate (Registry& registry, float dt) override;

    /**
     * @brief 销毁 ImGui 通用窗口系统。
     * @param registry 当前场景注册表
     */
    void OnDestroy(Registry& registry)           override;

private:
    // ── 核心窗口 ──────────────────────────────────────────────────────────
    /**
     * @brief 渲染主菜单栏。
     * @param registry 当前场景注册表
     */
    void RenderMainMenuBar  (Registry& registry);

    /**
     * @brief 渲染基础调试窗口。
     * @param registry 当前场景注册表
     * @param dt 本帧时间步长
     */
    void RenderDebugWindow  (Registry& registry, float dt);

    /**
     * @brief 渲染 NCL 运行状态窗口。
     * @param registry 当前场景注册表
     */
    void RenderNCLStatus    (Registry& registry);

    // ── Test Scene 调试控制（状态读写 Res_TestState context）────────────
    /**
     * @brief 渲染测试场景控制面板。
     * @param registry 当前场景注册表
     */
    void RenderTestControlsWindow(Registry& registry);

    /**
     * @brief 渲染网络调试面板。
     * @param registry 当前场景注册表
     */
    void RenderNetworkDebugWindow(Registry& registry);

    /**
     * @brief 生成一个测试方块实体。
     * @param registry 当前场景注册表
     */
    void SpawnCube(Registry& registry);

    /**
     * @brief 删除最后生成的测试方块实体。
     * @param registry 当前场景注册表
     */
    void DeleteLastCube(Registry& registry);

    /**
     * @brief 生成一个测试胶囊实体。
     * @param registry 当前场景注册表
     */
    void SpawnCapsule(Registry& registry);

    /**
     * @brief 删除最后生成的测试胶囊实体。
     * @param registry 当前场景注册表
     */
    void DeleteLastCapsule(Registry& registry);

    /**
     * @brief 批量设置测试实体的重力系数。
     * @param registry 当前场景注册表
     * @param factor 目标重力系数
     */
    void SetGravityAll(Registry& registry, float factor);

    /**
     * @brief 清理测试实体列表中的失效 EntityID。
     * @param registry 当前场景注册表
     * @param state 测试场景运行时状态
     */
    void CleanupTestEntities(Registry& registry, Res_TestState& state);

    // ── 窗口可见标志（系统配置，非游戏状态）────────────────────────────
    bool m_ShowDemoWindow  = false;
    bool m_ShowDebugWindow = true;
    bool m_ShowNCLStatus   = true;
    bool m_WireframeMode   = false;
};

} // namespace ECS
#endif
