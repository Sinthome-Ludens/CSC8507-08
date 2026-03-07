/**
 * @file C_D_Health.h
 * @brief 生命值数据组件，供 Sys_DeathJudgment 读取判定死亡。
 */
#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 生命值数据组件
 *
 * 存储实体的血量信息。hp <= 0 时由 Sys_DeathJudgment 判定为死亡。
 * invTimer > 0 期间不接受伤害（防连续伤害帧叠加）。
 * deathCause 记录死亡原因，由伤害来源在设置 hp=0 时同步写入。
 *
 * POD struct, 16 bytes.
 */
struct C_D_Health {
    float   hp         = 100.0f;  ///< 当前血量
    float   maxHp      = 100.0f;  ///< 最大血量
    float   invTimer   = 0.0f;    ///< 无敌计时器（秒），> 0 时不接受伤害
    uint8_t deathCause = 0;       ///< 死亡原因：0=被捕, 1=HP归零, 2=触发区即死, 3=敌人HP归零
};

} // namespace ECS
