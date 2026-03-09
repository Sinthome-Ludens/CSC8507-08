#pragma once

#include "Vector.h"

namespace ECS {

enum class ShadingModel : int {
    BlinnPhong = 0,
    PBR        = 1,
    Stylized   = 2
};

/// @brief 统一材质组件：PBR + Stylized + BlinnPhong 三种着色模型共存
struct C_D_Material {
    ShadingModel shadingModel = ShadingModel::BlinnPhong;

    // ── PBR 参数 (shadingModel == PBR) ──────────────────────
    float metallic  = 0.0f;
    float roughness = 0.5f;
    float ao        = 1.0f;

    // ── Stylized 参数 (shadingModel == Stylized) ────────────
    NCL::Maths::Vector3 emissiveColor    = {0, 0, 0};
    float               emissiveStrength = 0.0f;
    float               rimPower         = 3.0f;
    float               rimStrength      = 0.5f;
    bool                flatShading      = false;
};

} // namespace ECS
