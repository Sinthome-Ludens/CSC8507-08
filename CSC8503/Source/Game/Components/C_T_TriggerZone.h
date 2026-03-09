/**
 * @file C_T_TriggerZone.h
 * @brief Trigger 区域标签组件。
 *
 * @details
 * 用于标记仅参与 Trigger 检测的区域实体，便于游戏逻辑按标签筛选处理。
 */
#pragma once

namespace ECS {

/// @brief Trigger 区域标签（标签组件），标记仅用于重叠检测的实体
struct C_T_TriggerZone {};

} // namespace ECS
