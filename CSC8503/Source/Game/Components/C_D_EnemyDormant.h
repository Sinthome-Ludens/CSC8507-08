/**
 * @file C_D_EnemyDormant.h
 * @brief 敌人休眠状态组件（CQC 制服后标记）。
 */
#pragma once

namespace ECS {

/// @brief Enemy dormant state after CQC takedown (POD)
struct C_D_EnemyDormant {
    bool  isDormant       = false;
    float dormantTimer    = 0.0f;
};

} // namespace ECS
