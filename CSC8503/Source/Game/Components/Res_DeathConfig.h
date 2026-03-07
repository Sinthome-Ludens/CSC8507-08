#pragma once

namespace ECS {

/**
 * @brief 死亡判定系统全局配置资源（数据驱动）
 *
 * 注册到 Registry ctx，由 Sys_DeathJudgment 读取。
 * POD struct, 12 bytes.
 */
struct Res_DeathConfig {
    float captureDistance = 2.0f;   ///< Hunt 敌人抓捕玩家的 XZ 距离阈值（米）
    float invincibleTime = 0.5f;   ///< 受伤后无敌时间（秒）
    float restartDelay   = 0.0f;   ///< 死亡后重启延迟（秒），0 = 立即重启
};

} // namespace ECS
