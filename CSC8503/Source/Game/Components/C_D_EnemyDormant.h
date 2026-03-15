#pragma once

namespace ECS {

/// @brief Enemy dormant state after CQC takedown (POD)
struct C_D_EnemyDormant {
    bool  isDormant       = false;
    float dormantTimer    = 0.0f;
};

} // namespace ECS
