/**
 * @file MapPointsLoader.cpp
 * @brief .points 文件解析实现。
 */
#include "MapPointsLoader.h"
#include "Game/Utils/Log.h"

#include <fstream>
#include <sstream>

namespace ECS {

MapPointsData LoadMapPoints(const std::string& filePath)
{
    MapPointsData data;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        // .points 文件不存在是正常情况（非所有地图都有），不报警
        return data;
    }

    int startCount  = 0;
    int finishCount = 0;

    // ── 1. 解析头部：读取 startPointCount / finishPointCount ──────────
    //    兼容 .startpoints 格式：`count N` 等价于 `startPointCount N`
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if      (token == "startPointCount")  ss >> startCount;
        else if (token == "finishPointCount") ss >> finishCount;
        else if (token == "count")            ss >> startCount;  // .startpoints 格式
        else if (token == "startPoints")      break;
    }

    // ── 2. 读取 startPoints ──────────────────────────────────────────
    // 如果头部循环已经读到 "startPoints" 行则直接进入，否则继续找
    if (line.find("startPoints") == std::string::npos) {
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line.find("startPoints") != std::string::npos) break;
        }
    }

    int sRead = 0;
    while (sRead < startCount && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        float x, y, z;
        if (ss >> x >> y >> z) {
            data.startPoints.emplace_back(x, y, z);
            ++sRead;
        }
    }

    // ── 3. 读取 finishPoints（可选，.startpoints 格式无此节）─────────
    if (finishCount > 0) {
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line.find("finishPoints") != std::string::npos) break;
        }

        int fRead = 0;
        while (fRead < finishCount && std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            float x, y, z;
            if (ss >> x >> y >> z) {
                data.finishPoints.emplace_back(x, y, z);
                ++fRead;
            }
        }
    }

    data.loaded = true;
    LOG_INFO("[MapPointsLoader] Loaded " << filePath
             << " (" << data.startPoints.size() << " start, "
             << data.finishPoints.size() << " finish)");

    return data;
}

} // namespace ECS
