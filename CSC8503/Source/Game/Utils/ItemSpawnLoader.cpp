/**
 * @file ItemSpawnLoader.cpp
 * @brief .itemspawns 文件解析实现。
 */
#include "ItemSpawnLoader.h"
#include "Game/Utils/Log.h"
#include <fstream>
#include <sstream>

namespace ECS {

ItemSpawnData LoadItemSpawns(const std::string& filePath)
{
    ItemSpawnData result;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return result;
    }

    int spawnCount = 0;
    std::string line;

    // Read spawnCount header
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if (token == "spawnCount") {
            ss >> spawnCount;
            break;
        }
    }

    // Read each item entry
    for (int i = 0; i < spawnCount; ++i) {
        ItemSpawnEntry entry;
        bool entryComplete = false;

        while (!entryComplete && std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "item") {
                // item <index>, skip index
                continue;
            } else if (token == "itemId") {
                int id = 0;
                ss >> id;
                entry.itemId = static_cast<ItemID>(id);
            } else if (token == "position") {
                ss >> entry.position.x >> entry.position.y >> entry.position.z;
            } else if (token == "quantity") {
                int q = 1;
                ss >> q;
                entry.quantity = static_cast<uint8_t>(q);
                entryComplete = true; // quantity is the last field per entry
            }
        }

        result.spawns.push_back(entry);
    }

    result.loaded = (spawnCount > 0 && !result.spawns.empty());
    LOG_INFO("[ItemSpawnLoader] Loaded " << result.spawns.size()
             << "/" << spawnCount << " item spawns from " << filePath);
    return result;
}

} // namespace ECS
