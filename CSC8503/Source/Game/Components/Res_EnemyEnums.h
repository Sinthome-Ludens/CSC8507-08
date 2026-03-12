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
 * @brief 敌人 AI 状态枚举（由 alertLevel 驱动）。
 * @details
 * alertLevel → EnemyState 映射：
 *   - [0,  20) → Safe
 *   - [20, 40) → Search
 *   - [40, 80) → Alert
 *   - [80,100] → Hunt
 */
enum class EnemyState : uint8_t {
    Safe   = 0,   ///< 安全状态：巡逻或待机
    Search = 1,   ///< 搜索状态：警觉但未锁定
    Alert  = 2,   ///< 警戒状态：高度戒备
    Hunt   = 3    ///< 追捕状态：主动追杀玩家
};

}