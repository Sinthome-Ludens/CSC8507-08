/**
 * @file Res_GlobalAlertLevel.h
 * @brief 全局警戒度资源：存储游戏全局警戒等级（0-100，单调递增）
 *
 * @details
 * `Res_GlobalAlertLevel` 追踪游戏中所有敌人进入 Hunt 状态的累积效果。
 *
 * ## 警戒度规则
 *
 * - **初始值**：0
 * - **增量**：每当一个敌人进入 Hunt 状态时 +15
 * - **范围**：[0, 100]（超过 100 时截断）
 * - **特性**：单调递增，永不下降
 *
 * ## 使用示例
 *
 * @code
 * // 场景初始化（OnEnter）
 * registry.ctx_emplace<ECS::Res_GlobalAlertLevel>();
 *
 * // 读取警戒度
 * auto& alert = registry.ctx<ECS::Res_GlobalAlertLevel>();
 * int level = alert.alert_level;
 * @endcode
 *
 * @see Evt_AI_EnemyEnteredHunt
 */

#pragma once
#include <cstdint>

namespace ECS {

/**
 * @brief 全局警戒度资源：追踪游戏全局警戒等级。
 */
struct Res_GlobalAlertLevel {
    int alert_level = 0; ///< 当前全局警戒度 [0, 100]
};

} // namespace ECS
