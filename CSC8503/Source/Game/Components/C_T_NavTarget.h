#pragma once

namespace ECS {

/**
 * @brief 通用导航目标标签
 *
 * 任何挂载此组件的实体都可被 Sys_Navigation 识别为潜在目标。
 * Sys_Navigation 按 C_D_NavAgent::searchTag 与 targetType 做字符串匹配。
 *
 * ECS 合规说明：使用 char[32] 代替 std::string（组件不得含堆分配成员）。
 */
struct C_T_NavTarget {
    char targetType[32] = "Player";  ///< 目标类型标签（例如 "Player"、"WayPoint"）
};

} // namespace ECS
