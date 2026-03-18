/**
 * @file ItemSpawnLoader.h
 * @brief .itemspawns 文件解析工具：加载地图道具生成点。
 */
#pragma once

#include "Game/Components/C_D_Item.h"
#include "Vector.h"
#include <vector>
#include <string>

namespace ECS {

/**
 * @brief 单个道具生成点数据
 */
struct ItemSpawnEntry {
    ItemID               itemId   = ItemID::HoloBait; ///< 道具 ID
    NCL::Maths::Vector3  position = {};               ///< 生成位置（本地空间）
    uint8_t              quantity = 1;                 ///< 拾取数量
};

/**
 * @brief .itemspawns 文件解析结果
 */
struct ItemSpawnData {
    std::vector<ItemSpawnEntry> spawns;
    bool loaded = false;
};

/**
 * @brief 从 .itemspawns 文件加载道具生成点
 *
 * 文件格式：
 * @code
 * # 注释行
 * spawnCount N
 *
 * item 0
 * itemId 0
 * position x y z
 * quantity 1
 * @endcode
 *
 * @param filePath .itemspawns 文件路径
 * @return 解析结果；文件不存在时返回 loaded=false
 */
ItemSpawnData LoadItemSpawns(const std::string& filePath);

} // namespace ECS
