/**
 * @file C_D_NavAgent.h
 * @brief NavAgent 数据组件：导航移动参数、路径状态与状态感知字段。
 *
 * @details
 * 挂载条件：实体必须同时挂载 C_T_Pathfinder 才会被 Sys_Navigation 处理。
 * 路径使用固定大小数组（NAV_MAX_WAYPOINTS = 8），不使用 std::vector。
 */
#pragma once
#include "Vector.h"
#include "Quaternion.h"
#include "Game/Components/Res_EnemyEnums.h"

namespace ECS {

/// 单个 NavAgent 路径允许的最大路点数量
static constexpr int NAV_MAX_WAYPOINTS = 32;

/**
 * @brief NavAgent 数据组件 — 导航移动与路径状态
 *
 * ECS 合规说明：
 *   - 禁止使用 std::vector / std::string（组件必须为 POD 或接近 POD 类型）
 *   - 路径存储改为固定大小数组 path_waypoints[NAV_MAX_WAYPOINTS]
 *   - 目标标签改为 char search_tag[32]（代替 std::string）
 *
 * 挂载条件：实体必须同时挂载 C_T_Pathfinder 才会被 Sys_Navigation 处理。
 */
struct C_D_NavAgent {
    float speed             = 4.0f;    ///< Hunt 速度（< 玩家行走 5.0，< 玩家奔跑 7.5）
    float patrol_speed      = 2.5f;    ///< 巡逻/Alert 速度（= 玩家行走速度的一半）
    float rotation_speed    = 10.0f;  ///< 转向速度（数值越大转得越快）
    bool  smooth_rotation   = true;   ///< 是否开启平滑转向
    float stopping_distance     = 1.0f;
    float update_frequency      = 0.5f;
    float corner_decel_range    = 1.5f;
    float corner_decel_floor    = 0.4f;  ///< 转弯减速下限（速度百分比）
    float waypoint_arrival_sq   = 0.36f; ///< 非终点路点到达距离² (0.6m²)
    float obstacle_ray_height   = 0.5f;  ///< 障碍物检测射线 Y 偏移
    float obstacle_ray_range    = 1.5f;  ///< 障碍物检测距离 (m)
    float obstacle_check_min_dist = 0.5f; ///< 启用障碍物检测的最小目标距离 (m)
    float max_vertical_speed    = 8.0f;  ///< Y 轴速度限幅 (m/s)
    float timer             = 0.0f;

    /// 要追踪的目标实体标签（匹配 C_T_NavTarget::target_type）
    char search_tag[32] = "Player";

    /// 固定大小路径数组（替代 std::vector，最多 NAV_MAX_WAYPOINTS 个路点）
    NCL::Maths::Vector3 path_waypoints[NAV_MAX_WAYPOINTS];
    int  path_length              = 0;     ///< 当前路径有效路点数量
    int  current_waypoint_index   = 0;     ///< 当前正在追踪的路点下标
    bool is_active                = true;

    // ── 状态感知导航字段 ────────────────────────────────────────────────
    NCL::Maths::Vector3 last_known_target_pos{0.0f, 0.0f, 0.0f}; ///< 最后已知目标位置（Search 旋转用）
    NCL::Maths::Vector3 alert_snapshot_pos   {0.0f, 0.0f, 0.0f}; ///< 进入 Alert 时的位置快照
    EnemyState          prev_state = EnemyState::Safe;             ///< 上一帧 AI 状态（检测首次进入 Alert）
    bool                has_last_known_pos = false;                ///< 是否已记录过有效目标位置
    bool                search_arrived     = false;                ///< Search 状态下是否已到达调查点
};

} // namespace ECS
