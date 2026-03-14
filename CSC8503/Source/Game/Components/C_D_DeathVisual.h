/**
 * @file C_D_DeathVisual.h
 * @brief 死亡视觉参数组件，由 Sys_DeathEffect 每帧写入，由 Sys_Render 读取覆盖。
 */
#pragma once

#include "Vector.h"

namespace ECS {

struct C_D_DeathVisual {
    NCL::Maths::Vector4 colourOverride{1, 1, 1, 1}; ///< RGBA（Sys_DeathEffect 每帧写入）
    NCL::Maths::Vector3 originalScale{1, 1, 1};     ///< 初始化时记录原始缩放
    bool useTransparent = false;                     ///< 切换为透明材质
};

} // namespace ECS
