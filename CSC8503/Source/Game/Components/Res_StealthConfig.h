/**
 * @file Res_StealthConfig.h
 * @brief 玩家潜行指标配置资源（数据驱动，注册到 registry ctx）。
 */
#pragma once

namespace ECS {

/// @brief 玩家潜行指标参数配置（数据驱动，注册到 registry ctx）
struct Res_StealthConfig {
    // ── 姿态速度乘数 ──
    float stanceMulStanding   = 1.0f;
    float stanceMulCrouching  = 0.5f;

    // ── 伪装常量 ──
    float disguiseMul         = 0.3f;
    float noiseThrottle       = 0.3f;   ///< 噪音事件冷却时间（秒）

    // ── 伪装速度阈值（Sys_PlayerDisguise 用） ──
    float hideSpeedThreshold  = 1.0f;

    // ── 移动检测阈值 ──
    float moveDetectThreshold = 0.1f;   ///< 水平速度低于此值视为静止
};

} // namespace ECS
