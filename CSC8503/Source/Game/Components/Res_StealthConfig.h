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

    // ── 可见度 / 噪音指标（消除 Sys_StealthMetrics 内联 magic number）──
    // 伪装状态
    float disguiseVisMoving     = 0.4f;  ///< 伪装 + 移动时 visibilityFactor
    float disguiseVisStatic     = 0.0f;  ///< 伪装 + 静止时 visibilityFactor
    float disguiseNoiseMoving   = 0.5f;  ///< 伪装 + 移动时 noiseLevel
    float disguiseNoiseStatic   = 0.0f;  ///< 伪装 + 静止时 noiseLevel

    // 站立状态
    float standVisMoving        = 1.0f;
    float standVisStatic        = 0.7f;
    float standNoiseSprint      = 0.6f;
    float standNoiseWalk        = 0.2f;

    // 蹲伏状态
    float crouchVisMoving       = 0.5f;
    float crouchVisStatic       = 0.3f;
    float crouchNoiseSprint     = 0.6f;
    float crouchNoiseWalk       = 0.2f;
    float crouchNoiseMul        = 0.4f;  ///< 蹲伏噪音整体乘数

    // 噪音事件门槛
    float noiseEventThreshold   = 0.01f; ///< noiseLevel >= 此值才发布噪音事件
};

} // namespace ECS
