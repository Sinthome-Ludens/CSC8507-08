/**
 * @file Res_GameState.h
 * @brief 全局游戏状态资源：存储游戏进度、得分、敌人数量、暂停状态等全局状态
 *
 * @details
 * `Res_GameState` 存储当前游戏会话的全局状态，由多个 System 协作维护。
 *
 * ## 状态分类
 *
 * ### 1. 游戏进度状态
 * - `score`：玩家得分
 * - `currentLevel`：当前关卡编号
 * - `playerLives`：玩家剩余生命数
 *
 * ### 2. 敌人管理状态
 * - `enemyCount`：当前存活敌人数量
 * - `alertLevel`：全局警戒等级（0.0 ~ 1.0）
 *
 * ### 3. 系统控制状态
 * - `isPaused`：暂停标志
 * - `isGameOver`：游戏结束标志
 *
 * ## 维护责任
 *
 * - **Sys_Combat**：修改 `score`、`playerLives`
 * - **Sys_EnemySpawner**：修改 `enemyCount`
 * - **Sys_Alert**：修改 `alertLevel`
 * - **Sys_UI**：读取所有字段显示 HUD
 *
 * ## 使用示例
 *
 * @code
 * // Sys_Combat::OnEnemyKilled
 * void OnEnemyKilled(Registry& reg, EntityID enemy) {
 *     auto& state = reg.ctx<Res_GameState>();
 *     state.score += 100;
 *     state.enemyCount--;
 *
 *     if (state.enemyCount == 0) {
 *         // 触发关卡完成事件
 *     }
 * }
 * @endcode
 *
 * @note 多个 System 可能同时写入此资源，需注意逻辑顺序。
 */

#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 全局游戏状态资源：得分、敌人数量、暂停状态等。
 */
struct Res_GameState {
    uint32_t score        = 0;     ///< 玩家得分
    uint32_t currentLevel = 1;     ///< 当前关卡编号
    uint32_t playerLives  = 3;     ///< 玩家剩余生命数

    uint32_t enemyCount   = 0;     ///< 当前存活敌人数量
    float    alertLevel   = 0.0f;  ///< 全局警戒等级（0.0 ~ 1.0）

    bool isPaused   = false;       ///< 暂停标志
    bool isGameOver = false;       ///< 游戏结束标志
};

} // namespace ECS
