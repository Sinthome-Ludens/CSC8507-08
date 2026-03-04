#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"

/// @brief Published when a CQC takedown completes (POD)
struct Evt_CQC_Takedown {
    ECS::EntityID           player;
    ECS::EntityID           target;
    NCL::Maths::Vector3     position;
};
