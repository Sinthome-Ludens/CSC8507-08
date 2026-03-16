/**
 * @file EnemySpawnLoader.h
 * @brief .enemyspawns 文件解析工具：加载敌人生成点和巡逻路线。
 */
#pragma once

#include "Vector.h"
#include <vector>
#include <string>

namespace ECS {

/**
 * @brief 单个敌人生成点数据
 */
struct EnemySpawnEntry {
    std::string                      name;          ///< 生成点名称
    NCL::Maths::Vector3              position;      ///< 生成位置（本地空间）
    std::vector<NCL::Maths::Vector3> patrolPoints;  ///< 巡逻路点列表（本地空间）
};

/**
 * @brief .enemyspawns 文件解析结果
 */
struct EnemySpawnData {
    std::vector<EnemySpawnEntry> spawns;
    bool loaded = false;
};

/**
 * @brief 从 .enemyspawns 文件加载敌人生成点
 *
 * 文件格式（Unity 导出）：
 * @code
 * # 注释行
 * spawnCount N
 *
 * spawn 0
 * name SpawnPoint
 * position x y z
 * patrolCount M
 * patrol
 * x y z  × M
 * @endcode
 *
 * @param filePath .enemyspawns 文件路径
 * @return 解析结果；文件不存在时返回 loaded=false
 */
EnemySpawnData LoadEnemySpawns(const std::string& filePath);

} // namespace ECS
