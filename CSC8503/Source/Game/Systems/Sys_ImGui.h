#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief ImGui 渲染系统
 *
 * 职责：
 *   - 渲染全局菜单栏
 *   - 渲染性能 Debug 窗口（FPS / Frame Time / Entity Count）
 *   - 渲染 NCL Status 窗口（GameWorld 对象数 / Physics 状态）
 *   - 渲染通用基础控制面板（Cube/Capsule/Gravity，依赖 Res_BaseTestState）
 *   - 渲染 PhysicsTest 专属面板（Enemy/Target，依赖 Res_TestState）
 *   - 渲染 NavTest 专属面板（Enemy/Target，依赖 Res_NavTestState）
 *   - 渲染通用 AI 监控窗口（组件级，场景无关）
 *
 * 系统本身无状态（除窗口可见标志外），所有游戏数据存储于各 Res_* context。
 * 通过 has_ctx<> 检测当前场景注册了哪些 context 来决定渲染哪些窗口。
 *
 * 执行优先级：300（Render=200 之后）
 */
class Sys_ImGui : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    // ── 核心窗口（所有场景）──────────────────────────────────────────────
    void RenderMainMenuBar (Registry& registry);
    void RenderDebugWindow (Registry& registry, float dt);
    void RenderNCLStatus   (Registry& registry);

    // ── 通用基础层（所有场景，依赖 Res_BaseTestState）───────────────────
    void RenderBaseTestWindow(Registry& registry);   ///< Cube/Capsule/Gravity 控制面板
    void RenderCubeDebugWindow(Registry& registry);  ///< per-cube 详细 Debug 窗口

    void SpawnCube        (Registry& registry);
    void DeleteLastCube   (Registry& registry);
    void SpawnCapsule     (Registry& registry);
    void DeleteLastCapsule(Registry& registry);
    void SetGravityAll    (Registry& registry, float factor);

    // ── PhysicsTest 专属层（依赖 Res_TestState）─────────────────────────
    void RenderPhysicsTestWindow(Registry& registry); ///< PhysicsTest Enemy/Target 控制面板

    void SpawnEnemy     (Registry& registry);
    void DeleteLastEnemy(Registry& registry);

    // ── NavTest 专属层（依赖 Res_NavTestState）──────────────────────────
    void RenderNavTestWindow(Registry& registry);     ///< NavTest Enemy/Target 控制面板

    void SpawnEnemy_Nav    (Registry& registry);
    void DeleteLastEnemy_Nav(Registry& registry);
    void SpawnTarget       (Registry& registry);
    void DeleteLastTarget  (Registry& registry);

    // ── 通用 AI 监控（组件级，场景无关）────────────────────────────────
    void RenderEnemyAIStateWindow(Registry& registry);

    // ── 窗口可见标志（系统配置，非游戏状态）────────────────────────────
    bool m_ShowDemoWindow  = false;
    bool m_ShowDebugWindow = true;
    bool m_ShowNCLStatus   = true;
};

} // namespace ECS
#endif