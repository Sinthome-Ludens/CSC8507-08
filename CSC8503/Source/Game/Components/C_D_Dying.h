/**
 * @file C_D_Dying.h
 * @brief 死亡标记 + 计时器组件，挂载后触发死亡特效动画。
 *
 * 由 Sys_DeathJudgment 在敌人 hp<=0 时 Emplace，
 * 由 Sys_DeathEffect 读取并推进计时器，动画结束后销毁实体。
 */
#pragma once

namespace ECS {

struct C_D_Dying {
    float elapsed     = 0.0f;   ///< 已过时间（秒）
    float duration    = 0.8f;   ///< 总动画时长（秒）
    bool  initialized = false;  ///< 首帧初始化标记
};

} // namespace ECS
