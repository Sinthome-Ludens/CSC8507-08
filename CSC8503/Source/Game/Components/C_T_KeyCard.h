/**
 * @file C_T_KeyCard.h
 * @brief Key card pickup tag component (attached to collectible key entities in the world).
 */
#pragma once

#include <cstdint>

namespace ECS {

struct C_T_KeyCard {
    uint8_t keyId = 0; ///< Key ID for door pairing (0 = default)
};

} // namespace ECS
