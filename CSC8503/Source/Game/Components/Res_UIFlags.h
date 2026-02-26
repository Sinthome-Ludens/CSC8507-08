#pragma once

/**
 * @brief 全局 UI 显示标志（Registry Context 资源）
 *
 * 由 Sys_ImGui 的菜单栏控制，供各子系统读取以决定是否渲染对应窗口。
 * 在 Sys_TestScene::OnAwake 中通过 registry.ctx_emplace<Res_UIFlags>() 注册。
 */
struct Res_UIFlags {
    // ── 通用层（所有场景）────────────────────────────────────────────────
    bool showBaseTestControls    = true;  ///< 通用 Cube/Capsule/Gravity 控制面板
    bool showEnemyAIStatus       = true;  ///< 通用敌人状态监控表（组件级，场景无关）

    // ── PhysicsTest 专属层 ───────────────────────────────────────────────
    bool showPhysicsTestControls = true;  ///< PhysicsTest Enemy/Target 控制面板
    bool showCubeDebug           = true;  ///< PhysicsTest Cube 详细 Debug 窗口

    // ── NavTest 专属层 ───────────────────────────────────────────────────
    bool showNavTestControls     = true;  ///< NavTest Enemy/Target 控制面板

    // ── 已废弃（保留向后兼容，后续可清理）──────────────────────────────
    bool showTestControls   = true;  ///< @deprecated 改用 showPhysicsTestControls
    bool showEnemyAIControl = true;  ///< @deprecated 已拆分到各场景专属面板
};
