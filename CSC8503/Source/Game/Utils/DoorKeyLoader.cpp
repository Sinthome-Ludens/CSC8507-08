/**
 * @file DoorKeyLoader.cpp
 * @brief .doors file parser implementation.
 */
#include "DoorKeyLoader.h"
#include "Game/Utils/Log.h"
#include <fstream>
#include <sstream>

namespace ECS {

/**
 * @brief Parse a `.doors` placement file into DoorKeyData.
 *
 * Reads the header (doorCount, keyCount), then each door/key block.
 * Returns loaded=false if the file cannot be opened, the header is
 * malformed, or any entry is incomplete (missing keyId).
 *
 * @param filePath Full filesystem path to the .doors file.
 * @return Parsed door/key data; check `.loaded` before use.
 */
DoorKeyData LoadDoorKeys(const std::string& filePath)
{
    DoorKeyData result;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_WARN("[DoorKeyLoader] Cannot open: " << filePath);
        return result;
    }

    int doorCount = 0;
    int keyCount  = 0;
    bool foundDoorCount = false;
    bool foundKeyCount  = false;
    std::string line;

    // Read header: doorCount / keyCount
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token == "doorCount") {
            ss >> doorCount;
            foundDoorCount = true;
        } else if (token == "keyCount") {
            ss >> keyCount;
            foundKeyCount = true;
            break;
        }
    }

    if (!foundDoorCount || !foundKeyCount) {
        LOG_WARN("[DoorKeyLoader] Malformed header (missing doorCount/keyCount): " << filePath);
        return result;
    }

    // Read door entries
    for (int i = 0; i < doorCount; ++i) {
        DoorEntry entry;
        bool gotKeyId = false;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "door") {
                continue;
            } else if (token == "name") {
                std::getline(ss >> std::ws, entry.name);
            } else if (token == "position") {
                ss >> entry.position.x >> entry.position.y >> entry.position.z;
            } else if (token == "scale") {
                ss >> entry.scale.x >> entry.scale.y >> entry.scale.z;
            } else if (token == "keyId") {
                int id = 0;
                ss >> id;
                if (id < 0 || id > 255) {
                    LOG_WARN("[DoorKeyLoader] Door " << i << " keyId out of range: " << id);
                    id = 0;
                }
                entry.keyId = static_cast<uint8_t>(id);
                gotKeyId = true;
                break; // keyId is the last field per door
            }
        }
        if (!gotKeyId) {
            LOG_WARN("[DoorKeyLoader] Door " << i << " incomplete (missing keyId), skipping.");
            continue;
        }
        result.doors.push_back(std::move(entry));
    }

    // Read key entries
    for (int i = 0; i < keyCount; ++i) {
        KeyEntry entry;
        bool gotKeyId = false;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "key") {
                continue;
            } else if (token == "name") {
                std::getline(ss >> std::ws, entry.name);
            } else if (token == "position") {
                ss >> entry.position.x >> entry.position.y >> entry.position.z;
            } else if (token == "keyId") {
                int id = 0;
                ss >> id;
                if (id < 0 || id > 255) {
                    LOG_WARN("[DoorKeyLoader] Key " << i << " keyId out of range: " << id);
                    id = 0;
                }
                entry.keyId = static_cast<uint8_t>(id);
                gotKeyId = true;
                break; // keyId is the last field per key
            }
        }
        if (!gotKeyId) {
            LOG_WARN("[DoorKeyLoader] Key " << i << " incomplete (missing keyId), skipping.");
            continue;
        }
        result.keys.push_back(std::move(entry));
    }

    result.loaded = (!result.doors.empty() || !result.keys.empty());
    if (result.loaded) {
        LOG_INFO("[DoorKeyLoader] Loaded " << result.doors.size() << " doors, "
                 << result.keys.size() << " keys from " << filePath);
    } else {
        LOG_WARN("[DoorKeyLoader] No valid entries parsed from " << filePath);
    }
    return result;
}

} // namespace ECS
