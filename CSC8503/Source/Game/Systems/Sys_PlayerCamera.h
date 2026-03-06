#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Vector.h"

namespace ECS {

/**
 * @brief 第三人称跟随相机系统
 *
 * 职责：
 *   - 跟随 C_T_Player 实体，固定俯视角
 *   - 平滑 Lerp 插值相机位置
 *
 * 执行优先级：150（在 Sys_Physics=100 之后，Sys_Render=200 之前）
 */
class Sys_PlayerCamera : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

private:
    static constexpr float SMOOTH_SPEED = 17.0f;   ///< 插值速度因子
    static constexpr float FIXED_PITCH  = -75.0f;  ///< 固定俯仰角（度）
    static constexpr float FIXED_YAW    = 0.0f;    ///< 固定偏航角（度）

    /// 相机相对于玩家的偏移（世界坐标）
    const NCL::Maths::Vector3 CAMERA_OFFSET{0.0f, 25.0f, 6.7f};
};

} // namespace ECS
