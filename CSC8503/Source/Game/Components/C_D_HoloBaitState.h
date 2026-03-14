/**
 * @file C_D_HoloBaitState.h
 * @brief 全息诱饵炸弹激活状态组件（道具 001）。
 *
 * @details
 * 挂载在诱饵投掷物实体上（使用时由 Sys_ItemEffects 创建）。
 * Sys_ItemEffects 每帧更新 remainingTime，归零时删除该实体并重置敌人状态。
 *
 * ## 效果描述
 * 敌人处于安全状态时，投掷后敌人移动至道具位置；
 * 敌人抵达后约 3 秒恢复原状态。
 *
 * @see Sys_ItemEffects.h
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"

namespace ECS {

/**
 * @brief 全息诱饵炸弹激活状态数据组件
 *
 * 挂载在地图中投掷物实体上，由 Sys_ItemEffects 追踪生命周期。
 */
struct C_D_HoloBaitState {
    NCL::Maths::Vector3 worldPos;                ///< 诱饵在世界中的位置
    EntityID            attractedEnemy = Entity::NULL_ENTITY; ///< 被吸引的敌人实体（Entity::NULL_ENTITY = 尚未吸引）
    float               remainingTime  = 3.0f;   ///< 敌人抵达后恢复倒计时（秒）
    bool                enemyArrived   = false;  ///< 敌人是否已抵达诱饵位置
    bool                active         = true;   ///< 诱饵是否仍处于激活状态
};

} // namespace ECS
