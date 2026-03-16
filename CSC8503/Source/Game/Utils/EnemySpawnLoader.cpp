/**
 * @file EnemySpawnLoader.cpp
 * @brief .enemyspawns 文件解析实现。
 */
#include "EnemySpawnLoader.h"
#include "Game/Utils/Log.h"
#include <fstream>
#include <sstream>

namespace ECS {

EnemySpawnData LoadEnemySpawns(const std::string& filePath)
{
    EnemySpawnData result;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return result;
    }

    int spawnCount = 0;
    std::string line;

    // 读取 spawnCount
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

    // 逐个读取 spawn 条目
    for (int i = 0; i < spawnCount; ++i) {
        EnemySpawnEntry entry;
        int patrolCount = 0;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "spawn") {
                // spawn <index>，跳过 index
                continue;
            } else if (token == "name") {
                ss >> entry.name;
            } else if (token == "position") {
                ss >> entry.position.x >> entry.position.y >> entry.position.z;
            } else if (token == "patrolCount") {
                ss >> patrolCount;
            } else if (token == "patrol") {
                // 读取巡逻路点
                int pRead = 0;
                while (pRead < patrolCount && std::getline(file, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    std::istringstream pss(line);
                    NCL::Maths::Vector3 pt;
                    if (pss >> pt.x >> pt.y >> pt.z) {
                        entry.patrolPoints.push_back(pt);
                        ++pRead;
                    }
                }
                break; // patrol 是当前 spawn 的最后一节
            }
        }

        result.spawns.push_back(std::move(entry));
    }

    result.loaded = true;
    LOG_INFO("[EnemySpawnLoader] Loaded " << result.spawns.size()
             << " enemy spawns from " << filePath);
    return result;
}

} // namespace ECS
