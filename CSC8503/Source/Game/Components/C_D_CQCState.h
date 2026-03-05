#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <cstdint>

namespace ECS {

enum class CQCPhase : uint8_t {
    None     = 0,
    Approach = 1,
    Execute  = 2,
    Complete = 3
};

/// @brief Player CQC interaction state (POD)
struct C_D_CQCState {
    CQCPhase  phase         = CQCPhase::None;
    EntityID  targetEnemy   = 0;
    float     phaseTimer    = 0.0f;
    float     cooldown      = 0.0f;

    // Mimicry
    bool      isMimicking   = false;
    EntityID  mimicSource   = 0;
    MeshHandle     originalMesh = INVALID_HANDLE;
    MaterialHandle originalMat  = INVALID_HANDLE;
};

} // namespace ECS
