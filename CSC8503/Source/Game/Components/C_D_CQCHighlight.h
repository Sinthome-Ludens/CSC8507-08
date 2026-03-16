/**
 * @file C_D_CQCHighlight.h
 * @brief CQC 目标选中边缘高亮标记组件。
 */
#pragma once

#include "Vector.h"

namespace ECS {

/**
 * CQC 目标选中边缘高亮标记组件。
 * 由 Sys_PlayerCQC 在每帧管理挂载/移除，
 * 由 Sys_Render 在 SyncProxy 中读取并写入 GameTechMaterial rim 参数。
 * 渲染器将 rim 参数传入 scene.frag，生成边缘发光效果。
 */
struct C_D_CQCHighlight {
    NCL::Maths::Vector3 rimColour{0.85f, 0.87f, 0.9f};
    float rimPower    = 6.0f;
    float rimStrength = 0.35f;
};

} // namespace ECS
