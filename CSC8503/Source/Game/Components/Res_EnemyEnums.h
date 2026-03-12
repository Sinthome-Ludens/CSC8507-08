/**
 * @file Res_EnemyEnums.h
 * @brief 敌人状态枚举定义（EnemyState：Safe/Search/Alert/Hunt）。
 *
 * @details
 * 供 Sys_EnemyAI、Sys_Navigation、Sys_DeathJudgment 等共享使用。
 */
#pragma once
#include <cstdint>

namespace ECS {

/**
 * @brief 敌人 AI 状态枚举（由 detection_value 驱动）。
 * @details
 * C_D_AIPerception.detection_value → EnemyState 映射（默认阈值）：
 *   - [0,  20) → Safe   （search_threshold 以下）
 *   - [20, 40) → Search （search_threshold ~ alert_threshold）
 *   - [40, 80) → Alert  （alert_threshold ~ hunt_threshold）
 *   - [80,100] → Hunt   （hunt_threshold 以上）
 * 阈值由 C_D_AIPerception 的各 *_threshold 字段控制，可在运行时调节。
 * @note 与 Res_GameState::AlertStatus 同名状态的阈值不同（AlertStatus 驱动于全局 gs.alertLevel）
 */
enum class EnemyState : uint8_t {
    Safe   = 0,   ///< 安全状态：巡逻或待机
    Search = 1,   ///< 搜索状态：警觉但未锁定
    Alert  = 2,   ///< 警戒状态：高度戒备
    Hunt   = 3    ///< 追捕状态：主动追杀玩家
};

}