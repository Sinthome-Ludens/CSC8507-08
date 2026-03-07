#pragma once

namespace ECS {

/**
 * @brief 生命值数据组件
 *
 * 存储实体的血量信息。hp <= 0 时由 Sys_DeathJudgment 判定为死亡。
 * invTimer > 0 期间不接受伤害（防连续伤害帧叠加）。
 *
 * POD struct, 12 bytes.
 */
struct C_D_Health {
    float hp       = 100.0f;  ///< 当前血量
    float maxHp    = 100.0f;  ///< 最大血量
    float invTimer = 0.0f;    ///< 无敌计时器（秒），> 0 时不接受伤害
};

} // namespace ECS
