/**
 * @file Sys_Navigation.h
 * @brief 导航系统声明（ECS 系统，优先级 130）。
 *
 * @see Sys_Navigation.cpp
 */
#pragma once
#include "Core/ECS/BaseSystem.h"
#include "Game/Utils/PathfinderUtil.h"

namespace ECS {

/**
 * @brief 导航系统 — 状态感知寻路与移动控制
 *
 * 处理所有同时挂载 C_T_Pathfinder + C_D_NavAgent 的实体。
 * 根据 C_D_AIState 当前状态分支执行不同移动策略：
 *   Safe    — 完全静止
 *   Search — 停止移动，朝向最后已知目标旋转
 *   Alert   — 移动到进入 Alert 时的目标位置快照
 *   Hunt    — 实时追踪目标，定期重新规划路径
 *
 * 依赖：
 *   - Sys_Physics（从 registry.ctx<Sys_Physics*>() 获取，供速度/旋转控制）
 *   - PathfinderUtil（通过 SetPathfinder() 注入）
 *
 * 执行优先级：130（晚于 Sys_EnemyAI 120，确保读取最新 AI 状态）
 */
class Sys_Navigation : public ISystem {
public:
    Sys_Navigation() = default;

    /// 注入寻路器（在场景 OnEnter 中调用，注入 NavMeshPathfinderUtil 实例）
    void SetPathfinder(PathfinderUtil* pf) { m_Pathfinder = pf; }

    /**
     * @brief 每帧推进所有 C_T_Pathfinder + C_D_NavAgent 实体的导航逻辑。
     * @param registry ECS 注册表
     * @param dt       帧时间（秒）
     */
    void OnUpdate(Registry& registry, float dt) override;

private:
    PathfinderUtil* m_Pathfinder = nullptr;
};

} // namespace ECS
