/**
 * @file C_D_Health.h
 * @brief 生命值数据组件与死亡类型枚举，供 Sys_DeathJudgment 读取判定死亡。
 */
#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 实体死亡类型枚举
 *
 * 由伤害来源写入 C_D_Health::deathCause，
 * 由 Sys_DeathJudgment 读取后填入 Evt_Death::deathType 发布。
 */
enum class DeathType : uint8_t {
    PlayerCaptured   = 0, ///< 玩家被 Hunt 敌人抓捕
    PlayerHpZero     = 1, ///< 玩家 HP 归零（通用伤害）
    PlayerTriggerDie = 2, ///< 玩家进入即死触发区
    EnemyHpZero      = 3  ///< 敌人 HP 归零
};

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
    float     hp         = 100.0f;                    ///< 当前血量
    float     maxHp      = 100.0f;                    ///< 最大血量
    float     invTimer   = 0.0f;                      ///< 无敌计时器（秒），> 0 时不接受伤害
    DeathType deathCause = DeathType::PlayerCaptured;  ///< 死亡原因（由伤害来源写入）
};

} // namespace ECS
