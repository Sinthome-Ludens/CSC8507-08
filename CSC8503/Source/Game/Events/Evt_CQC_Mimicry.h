#pragma once

#include "Core/ECS/EntityID.h"

/// @brief Published when mimicry is activated or revoked (POD)
struct Evt_CQC_Mimicry {
    ECS::EntityID player;
    ECS::EntityID source;
    bool          activated;
};
