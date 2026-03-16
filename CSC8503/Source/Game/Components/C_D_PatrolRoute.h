/**
 * @file C_D_PatrolRoute.h
 * @brief 巡逻路线数据组件：存储巡逻路点列表与当前巡逻进度。
 *
 * 挂载条件：实体需同时挂载 C_D_NavAgent + C_T_Pathfinder 才会被 Sys_Navigation 处理。
 * 在 Safe 状态下，Sys_Navigation 读取本组件驱动巡逻行为。
 */
#pragma once
#include "Vector.h"

namespace ECS {

/// 单条巡逻路线允许的最大路点数量
static constexpr int PATROL_MAX_WAYPOINTS = 16;

/**
 * @brief 巡逻路线数据组件（POD）
 *
 * 路点坐标为世界空间（场景加载时已完成缩放和偏移）。
 * Sys_Navigation 在 Safe 状态下依次寻路到每个路点，到达后推进到下一个并循环。
 */
struct C_D_PatrolRoute {
    NCL::Maths::Vector3 waypoints[PATROL_MAX_WAYPOINTS];
    int  count              = 0;     ///< 有效路点数量
    int  current_index      = 0;     ///< 当前目标路点下标
    bool needs_path         = true;  ///< 是否需要规划到当前路点的路径
};

} // namespace ECS
