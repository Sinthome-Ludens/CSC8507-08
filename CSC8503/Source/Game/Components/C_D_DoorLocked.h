/**
 * @file C_D_DoorLocked.h
 * @brief Locked door data component (attached to door entities that require a key card).
 */
#pragma once

#include <cstdint>

namespace ECS {

struct C_D_DoorLocked {
    uint8_t requiredKeyId = 0; ///< Key ID required to unlock this door
};

} // namespace ECS
