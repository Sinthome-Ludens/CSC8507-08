/**
 * @file RuntimeOverrides.h
 * @brief 运行时参数覆盖结构体，传递 JSON 无法承载的数据。
 *
 * @details
 * PrefabFactory::Create() 通用入口使用此结构体接收运行时参数：
 * mesh 句柄、生成位置、碰撞几何等无法在 JSON 中静态声明的数据。
 * 各字段为 std::optional，仅设置的字段覆盖 JSON 默认值。
 */
#pragma once

#include "Vector.h"
#include "Quaternion.h"
#include "Game/Components/C_D_MeshRenderer.h"  // MeshHandle typedef

#include <optional>
#include <vector>

namespace ECS {

struct RuntimeOverrides {
    std::optional<NCL::Maths::Vector3>     position;      // 生成位置覆盖
    std::optional<NCL::Maths::Quaternion>  rotation;      // 旋转覆盖
    std::optional<NCL::Maths::Vector3>     scale;         // 缩放覆盖
    std::optional<MeshHandle>              meshHandle;    // AssetManager 返回的句柄
    std::optional<int>                     spawnIndex;    // DebugName 编号 (_XX → _03)
    std::optional<NCL::Maths::Vector3>     halfExtents;   // 碰撞体半尺寸覆盖
    std::optional<NCL::Maths::Vector3>     worldOffset;   // 地图 Y 偏移覆盖 position
    std::optional<NCL::Maths::Vector3>     targetPos;     // HoloBait/RoamAI 目标位置

    const std::vector<NCL::Maths::Vector3>* triVerts   = nullptr;  // TriMesh 顶点
    const std::vector<int>*                 triIndices = nullptr;  // TriMesh 索引
};

} // namespace ECS
