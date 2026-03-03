#pragma once

namespace ECS {

/**
 * @brief 寻路启用标签
 *
 * 只要挂载此组件，Sys_Navigation 就会处理该实体的移动逻辑。
 * 空结构体，仅作为标识用途（符合 C_T_* 标签组件规范）。
 */
struct C_T_Pathfinder {};

} // namespace ECS
