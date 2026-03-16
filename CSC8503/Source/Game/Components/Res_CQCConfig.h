/**
 * @file Res_CQCConfig.h
 * @brief CQC 系统配置资源（数据驱动，注册到 registry ctx）。
 */
#pragma once

#include "Vector.h"

namespace ECS {

/// @brief CQC configuration resource (data-driven, registered to registry ctx)
struct Res_CQCConfig {
    float maxDistance      = 5.0f;
    float cooldownTime    = 1.0f;
    float approachTime    = 0.2f;
    float executeTime     = 0.8f;

    // 选中目标边缘高亮参数
    NCL::Maths::Vector3 highlightRimColour{0.85f, 0.87f, 0.9f};
    float highlightRimPower    = 6.0f;
    float highlightRimStrength = 0.35f;
};

} // namespace ECS
