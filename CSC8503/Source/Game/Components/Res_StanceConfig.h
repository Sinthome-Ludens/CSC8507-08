/**
 * @file Res_StanceConfig.h
 * @brief 玩家姿态碰撞体配置资源（数据驱动，注册到 registry ctx）。
 */
#pragma once

namespace ECS {

/// @brief 玩家姿态碰撞体参数配置（数据驱动，注册到 registry ctx）
struct Res_StanceConfig {
    // ── 站立 Box 半尺寸 ──
    float standHalfX  = 1.0f;
    float standHalfY  = 1.0f;
    float standHalfZ  = 1.0f;

    // ── 蹲伏 Box 半尺寸 ──
    float crouchHalfX = 1.0f;
    float crouchHalfY = 0.5f;
    float crouchHalfZ = 1.0f;

    // ── 碰撞偏移 ──
    float skinOffset  = 0.05f;  ///< 防止碰撞体嵌入地面
};

} // namespace ECS
