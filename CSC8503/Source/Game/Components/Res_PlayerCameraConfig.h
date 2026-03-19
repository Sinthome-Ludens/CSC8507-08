/**
 * @file Res_PlayerCameraConfig.h
 * @brief 玩家跟随相机配置资源（数据驱动，注册到 registry ctx）。
 */
#pragma once

#include "Vector.h"

namespace ECS {

/// @brief 玩家跟随相机参数配置（数据驱动，注册到 registry ctx）
struct Res_PlayerCameraConfig {
    float smoothSpeed = 17.0f;                                ///< 插值速度因子
    float fixedPitch  = -75.0f;                               ///< 固定俯仰角（度）
    float fixedYaw    = 0.0f;                                 ///< 固定偏航角（度）
    NCL::Maths::Vector3 cameraOffset{0.0f, 25.0f, 6.7f};     ///< 相机相对玩家偏移
};

} // namespace ECS
