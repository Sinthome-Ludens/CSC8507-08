/**
 * @file Res_OrbitConfig.h
 * @brief 环绕三角形系统配置资源（ctx resource）。
 */
#pragma once

#include "Game/Components/C_D_MeshRenderer.h" // MeshHandle

namespace ECS {

struct Res_OrbitConfig {
    // ── Orbit 参数 ──
    float orbitRadius    = 2.0f;   ///< 环绕半径（m）
    float orbitHeight    = 1.5f;   ///< 环绕高度（相对玩家 Y）
    float orbitSpeed     = 1.5f;   ///< 公转角速度（rad/s）

    // ── 弹簧阻尼 ──
    float springStiffness = 25.0f;  ///< 弹簧刚度
    float springDamping   = 8.0f;   ///< 阻尼系数

    // ── 悬浮 + 自转 ──
    float hoverAmplitude = 0.15f;   ///< 悬浮幅度（m）
    float hoverFreq      = 2.5f;    ///< 悬浮频率（Hz）
    float spinSpeed      = 3.0f;    ///< 自转角速度（rad/s）
    float maxOrbitVelocity = 50.0f; ///< 弹簧阻尼最大速度 clamp（m/s）

    // ── Projectile 参数 ──
    float projectileSpeed    = 30.0f; ///< 弹射飞行速度
    float projectileTurnRate = 8.0f;  ///< 制导转向速率（rad/s）
    float projectileLife     = 5.0f;  ///< 弹射体最大存活时间
    float hitRadius          = 2.5f;  ///< 命中判定半径（m）

    // ── Mesh 缓存 ──
    MeshHandle meshHandle = 0; ///< 三角形模型 Handle（Sys_OrbitTriangle::OnAwake 初始化）
};

} // namespace ECS
