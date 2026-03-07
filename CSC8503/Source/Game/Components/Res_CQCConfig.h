#pragma once

namespace ECS {

/// @brief CQC configuration resource (data-driven, registered to registry ctx)
struct Res_CQCConfig {
    float maxDistance      = 5.0f;
    float dorsalDotMin    = 0.5f;
    float cooldownTime    = 1.0f;
    float approachTime    = 0.2f;
    float executeTime     = 0.8f;
    float mimicryDistance  = 5.0f;
};

} // namespace ECS
