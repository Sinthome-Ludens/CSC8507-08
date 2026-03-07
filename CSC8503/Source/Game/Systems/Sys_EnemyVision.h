/**
 * @file Sys_EnemyVision.h
 * @brief 敌人视野判定系统声明（ECS 系统，优先级 110）。
 *
 * 每帧为每个活跃敌人计算视野检测结果，写入 C_D_AIPerception::is_spotted，
 * 供 Sys_EnemyAI 的警戒度状态机消费。配置通过 Res_VisionConfig 数据驱动。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/// @brief 敌人视野判定系统（优先级 110）
///
/// 职责：每帧为每个活跃敌人计算 is_spotted，XZ 平面扇形视锥 + 射线遮挡
/// 读：C_D_Transform, C_D_PlayerState, C_D_CQCState, C_T_Player, C_T_Enemy,
///     C_D_EnemyDormant, Res_VisionConfig, Sys_Physics*
/// 写：C_D_AIPerception.is_spotted
class Sys_EnemyVision : public ISystem {
public:
    /**
     * @brief 每帧更新敌人视野状态。
     *
     * 对每个活跃敌人执行 5 层检测：XZ 距离 → visibilityFactor 门槛 →
     * 近距离 360° 感知 → 扇形视锥（点积）→ CastRay 遮挡，
     * 写入 C_D_AIPerception::is_spotted。
     *
     * @param registry ECS 注册表，用于访问敌人、玩家及相关组件与全局资源。
     * @param dt 本帧经过的时间（秒）。
     */
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
