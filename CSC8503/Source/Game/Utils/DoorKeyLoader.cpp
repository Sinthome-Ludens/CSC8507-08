/**
 * @file DoorKeyLoader.cpp
 * @brief .doors file parser implementation.
 */
#include "DoorKeyLoader.h"
#include "Game/Utils/Log.h"
#include <fstream>
#include <sstream>

namespace ECS {

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
    std::string line;

    // Read header: doorCount / keyCount
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token == "doorCount") {
            ss >> doorCount;
        } else if (token == "keyCount") {
            ss >> keyCount;
            break;
        }
    }

    // Read door entries
    for (int i = 0; i < doorCount; ++i) {
        DoorEntry entry;
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
                entry.keyId = static_cast<uint8_t>(id);
                break; // keyId is the last field per door
            }
        }
        result.doors.push_back(std::move(entry));
    }

    // Read key entries
    for (int i = 0; i < keyCount; ++i) {
        KeyEntry entry;
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
                entry.keyId = static_cast<uint8_t>(id);
                break; // keyId is the last field per key
            }
        }
        result.keys.push_back(std::move(entry));
    }

    result.loaded = true;
    LOG_INFO("[DoorKeyLoader] Loaded " << result.doors.size() << " doors, "
             << result.keys.size() << " keys from " << filePath);
    return result;
}

} // namespace ECS
