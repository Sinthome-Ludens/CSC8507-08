/**
 * @file Sys_Spin.h
 * @brief Orb 自旋与位置跟随系统：驱动玩家/敌人 Orb 每帧更新位置并旋转。
 *
 * @details
 * 每帧处理两类实体：
 *  1. `C_T_OrbOfPlayer + C_D_Spin + C_D_Transform`：同步到玩家位置 + Y 轴旋转
 *  2. `C_T_OrbOfEnemy  + C_D_Spin + C_D_Transform`：同步到对应敌人位置 + Y 轴旋转
 *
 * 旋转逻辑：
 *  - `speed == 0`：仅同步位置（外层装饰球壳）
 *  - `speed != 0`：位置同步后，每帧累加 `AxisAngleToQuaterion({0,1,0}, speed * dt)` 旋转
 *
 * 执行优先级：66（在 Sys_Movement=65 之后、Sys_Physics=100 之前）
 * 支持暂停：使用 PAUSE_GUARD 宏，与其他系统保持一致。
 *
 * @see C_D_Spin
 * @see C_T_OrbOfPlayer
 * @see C_T_OrbOfEnemy
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief Orb 自旋与跟随系统。
 *
 * 无内部状态，所有运行时数据存于 C_D_Spin 和 C_D_Transform 组件。
 */
class Sys_Spin : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
