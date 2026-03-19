/**
 * @file Res_MovementConfig.h
 * @brief 玩家移动参数配置资源（数据驱动，注册到 registry ctx）。
 */
#pragma once

namespace ECS {

/// @brief 玩家移动参数配置（数据驱动，注册到 registry ctx）
struct Res_MovementConfig {
    float baseSpeed    = 5.0f;    ///< 基准速度（站立行走）
    float baseForce    = 80.0f;   ///< 基准驱动力
    float runSpeedMul  = 1.5f;    ///< 奔跑倍率 → 7.5
};

} // namespace ECS
