/**
 * @file C_T_RoamAI.h
 * @brief 流窜 AI 标签组件（道具 004）。
 *
 * @details
 * 挂载在由"流窜 AI"道具释放的巡逻 AI 实体上。
 * Sys_ItemEffects 通过此标签识别流窜 AI 实体，执行随机巡逻与碰撞消灭逻辑。
 *
 * @see C_D_RoamAI.h
 * @see Sys_ItemEffects.h
 */
#pragma once

namespace ECS {

/**
 * @brief 流窜 AI 实体标签组件（无数据，仅用于识别）
 */
struct C_T_RoamAI {};

} // namespace ECS
