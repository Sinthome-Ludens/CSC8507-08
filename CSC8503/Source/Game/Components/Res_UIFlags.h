#pragma once

/**
 * @brief 全局 UI 显示标志（Registry Context 资源）
 *
 * 由 Sys_ImGui 的菜单栏控制，供各子系统读取以决定是否渲染对应窗口。
 * 在 Sys_TestScene::OnAwake 中通过 registry.ctx_emplace<Res_UIFlags>() 注册。
 */
struct Res_UIFlags {
    bool showTestControls = true;  ///< 是否显示 "ECS Test Controls" 控制面板
    bool showCubeDebug    = true;  ///< 是否显示 "Cube Debug Info" 浮动 debug 窗口
    bool showEnemyAIControl = true;
    bool showEnemyAIStatus =  true;
};
