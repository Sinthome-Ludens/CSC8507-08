#pragma once

#include "Core/ECS/BaseSystem.h"

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
};

} // namespace ECS
