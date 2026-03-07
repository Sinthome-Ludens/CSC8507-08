#pragma once

/**
 * @brief 即死触发区域标签组件
 *
 * 附加在 is_trigger=true 的碰撞体实体上。
 * 当玩家进入该触发器时，Sys_DeathJudgment 将其 HP 设为 0（即死）。
 */
struct C_T_DeathZone {};
