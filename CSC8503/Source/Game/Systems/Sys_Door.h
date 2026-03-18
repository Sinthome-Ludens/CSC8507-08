/**
 * @file Sys_Door.h
 * @brief Key card pickup + locked door system (priority 270, after Sys_Item).
 *
 * OnUpdate: scans C_T_KeyCard entities, auto-collects within XZ range,
 * then destroys the key and all C_D_DoorLocked entities with matching keyId.
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_Door : public ISystem {
public:
    /** @brief Initialise door/key system (no-op). */
    void OnAwake(Registry& registry) override;

    /**
     * @brief Per-frame key pickup: collect keys within range, destroy keys
     *        and all matching locked doors.
     */
    void OnUpdate(Registry& registry, float dt) override;

private:
    static constexpr float kKeyPickupRadius = 2.0f;  ///< XZ pickup radius (metres)
};

} // namespace ECS
