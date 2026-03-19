/**
 * @file Sys_GhostDuel.h
 * @brief Ghost Duel system: updates ghost entity transform from network-synced position data.
 *
 * @details
 * Priority 56 (immediately after Sys_Network 54, before Sys_InputDispatch 55).
 * Reads Res_GhostDuelState ghost position/visibility and writes to the ghost entity's C_D_Transform.
 */
#pragma once

#include "Core/ECS/SystemManager.h"

namespace ECS {

class Sys_GhostDuel : public ISystem {
public:
    void OnAwake(Registry& reg) override;
    void OnUpdate(Registry& reg, float dt) override;
    void OnDestroy(Registry& reg) override;
};

} // namespace ECS
