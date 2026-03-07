/**
 * @file Res_DeathConfig.h
 * @brief 死亡判定系统全局配置资源，注册到 Registry context 中供 Sys_DeathJudgment 读取。
 */
#pragma once

namespace ECS {

/**
 * @brief 死亡判定系统全局配置资源（数据驱动）
 *
 * 注册到 Registry ctx，由 Sys_DeathJudgment 读取。
 * POD struct, 4 bytes.
 */
struct Res_DeathConfig {
    float captureDistance = 2.0f;   ///< Hunt 敌人抓捕玩家的 XZ 距离阈值（米）
};

} // namespace ECS
