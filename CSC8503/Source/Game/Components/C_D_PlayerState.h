#pragma once

#include <cstdint>

namespace ECS {

/// @brief 玩家姿态枚举
enum class PlayerStance : uint8_t {
    Standing  = 0, ///< 站立（默认）
    Crouching = 1, ///< 蹲伏
    Prone     = 2  ///< 匍匐
};

/// @brief 贴墙状态枚举
enum class WallState : uint8_t {
    None     = 0, ///< 未贴墙
    Pressing = 1, ///< 贴墙中
    Peeking  = 2  ///< 探头（预留）
};

/**
 * @brief 玩家状态数据组件（MGS 风格潜行系统）
 *
 * 存储姿态、噪音、可见度、贴墙等潜行相关数据。
 * 由多个系统写入：Sys_PlayerStance（姿态与碰撞体）、Sys_StealthMetrics（噪音/可见度等潜行指标）、Sys_PlayerDisguise（伪装与强制站起标志）等；供 AI 守卫及其他相关游戏逻辑读取。
 */
struct C_D_PlayerState {
    // ── 姿态 ──
    PlayerStance stance        = PlayerStance::Standing;
    bool         isSprinting   = false;
    float        moveSpeedMul  = 1.0f; ///< 综合速度乘数（姿态 × 伪装）

    // ── 潜行指标 ──
    float noiseLevel      = 0.0f; ///< 当前噪音等级 [0, 1]
    float visibilityFactor = 1.0f; ///< 当前可见度因子 [0, 1]

    // ── 贴墙 ──
    WallState wallState   = WallState::None;
    float     wallNormalX = 0.0f; ///< 墙面法线 X
    float     wallNormalZ = 0.0f; ///< 墙面法线 Z
    float     wallPointX  = 0.0f; ///< 墙面接触点 X
    float     wallPointZ  = 0.0f; ///< 墙面接触点 Z

    // ── 伪装 ──
    bool isDisguised = false;

    // ── 强制站起标志（由 Sys_PlayerDisguise / Sys_StealthMetrics 设置，Sys_PlayerStance 执行后重置） ──
    bool forceStandPending = false;

    // ── 噪音节流（per-entity） ──
    float noiseCooldown = 0.0f;

    // ── 碰撞体参数（当前姿态） ──
    float colliderRadius     = 0.5f; ///< 胶囊半径
    float colliderHalfHeight = 1.0f; ///< 胶囊半高
};

} // namespace ECS
