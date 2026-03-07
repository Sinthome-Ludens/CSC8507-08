/**
 * @file C_T_DeathZone.h
 * @brief 即死触发区域标签组件，用于 Sys_DeathJudgment 的触发器即死检测。
 */
#pragma once

namespace ECS {

/**
 * @brief 即死触发区域标签组件
 *
 * 附加在 is_trigger=true 的碰撞体实体上。
 * 当玩家进入该触发器时，Sys_DeathJudgment 将其 HP 设为 0（即死）。
 */
struct C_T_DeathZone {};

} // namespace ECS
