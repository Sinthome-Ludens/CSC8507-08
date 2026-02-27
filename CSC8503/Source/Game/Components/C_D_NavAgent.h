#pragma once
#include "Vector.h"
#include "Quaternion.h"
#include "Game/Components/Res_EnemyEnums.h"

namespace ECS {

/// 单个 NavAgent 路径允许的最大路点数量
static constexpr int NAV_MAX_WAYPOINTS = 8;

/**
 * @brief NavAgent 数据组件 — 导航移动与路径状态
 *
 * ECS 合规说明：
 *   - 禁止使用 std::vector / std::string（组件必须为 POD 或接近 POD 类型）
 *   - 路径存储改为固定大小数组 pathWaypoints[NAV_MAX_WAYPOINTS]
 *   - 目标标签改为 char searchTag[32]（代替 std::string）
 *
 * 挂载条件：实体必须同时挂载 C_T_Pathfinder 才会被 Sys_Navigation 处理。
 */
struct C_D_NavAgent {
    float speed            = 5.0f;
    float rotationSpeed    = 10.0f;  ///< 转向速度（数值越大转得越快）
    bool  smoothRotation   = true;   ///< 是否开启平滑转向
    float stoppingDistance = 1.0f;
    float updateFrequency  = 0.5f;
    float timer            = 0.0f;

    /// 要追踪的目标实体标签（匹配 C_T_NavTarget::targetType）
    char searchTag[32] = "Player";

    /// 固定大小路径数组（替代 std::vector，最多 NAV_MAX_WAYPOINTS 个路点）
    NCL::Maths::Vector3 pathWaypoints[NAV_MAX_WAYPOINTS];
    int pathLength             = 0;   ///< 当前路径有效路点数量
    int currentWaypointIndex   = 0;   ///< 当前正在追踪的路点下标
    bool isActive              = true;

    // ── 状态感知导航字段 ────────────────────────────────────────────────
    NCL::Maths::Vector3 lastKnownTargetPos{0.0f, 0.0f, 0.0f}; ///< 最后已知目标位置（Caution 旋转用）
    NCL::Maths::Vector3 alertSnapshotPos  {0.0f, 0.0f, 0.0f}; ///< 进入 Alert 时的位置快照
    EnemyState          prevState = EnemyState::Safe;           ///< 上一帧 AI 状态（检测首次进入 Alert）
    bool                hasLastKnownPos = false;                ///< 是否已记录过有效目标位置
};

} // namespace ECS
