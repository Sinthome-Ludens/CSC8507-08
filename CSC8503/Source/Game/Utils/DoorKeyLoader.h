/**
 * @file DoorKeyLoader.h
 * @brief .doors file parser: loads door and key card placement data.
 */
#pragma once

#include "Vector.h"
#include <vector>
#include <string>
#include <cstdint>

namespace ECS {

struct DoorEntry {
    std::string         name;
    NCL::Maths::Vector3 position;
    NCL::Maths::Vector3 scale;     ///< Door dimensions (x=thickness, y=height, z=width)
    uint8_t             keyId = 0;
};

struct KeyEntry {
    std::string         name;
    NCL::Maths::Vector3 position;
    uint8_t             keyId = 0;
};

struct DoorKeyData {
    std::vector<DoorEntry> doors;
    std::vector<KeyEntry>  keys;
    bool loaded = false;
};

/**
 * @brief Load door and key placement data from a .doors file.
 * @param filePath Full path to the .doors file.
 * @return Parsed data; loaded=false if the file cannot be opened,
 *         the header is malformed (missing doorCount/keyCount),
 *         or no valid entries were parsed.
 */
DoorKeyData LoadDoorKeys(const std::string& filePath);

} // namespace ECS
