#pragma once

#include "Vector.h"

/// @brief 全局 Stylized 默认参数（新建实体时使用）
struct Res_StylizedDefaults {
    NCL::Maths::Vector3 emissiveColor    = {0, 0, 0};
    float               emissiveStrength = 0.0f;
    float               rimPower         = 3.0f;
    float               rimStrength      = 0.5f;
    bool                flatShading      = false;
};
