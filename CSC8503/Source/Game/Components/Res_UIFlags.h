/**
 * @file Res_UIFlags.h
 * @brief 全局 UI 窗口显隐标志资源定义。
 */
#pragma once

#include <cstdint>

/**
 * @brief 全局 UI 显示标志（Registry Context 资源）
 *
 * 由 Sys_ImGui 的菜单栏控制，供各子系统读取以决定是否渲染对应窗口。
 * 在 Sys_TestScene::OnAwake 中通过 registry.ctx_emplace<Res_UIFlags>() 注册。
 */
struct Res_UIFlags {
    bool showTestControls = true;  ///< 是否显示 "ECS Test Controls" 控制面板
    bool showEntityDebug  = true;  ///< 是否显示 "Entity Debug Info" 全量实体调试窗口
    bool showNetworkDebug = true;  ///< 是否显示 "Network Debug" 控制面板

    int8_t debugSceneIndex = -1;  ///< -1=无请求, 0=MainMenu, 1=PhysicsTest, 2=NavTest, 3=NetworkGame(Server)
    bool showRaycast      = true;  ///< 是否显示 "Raycast" 控制面板
};
