/**
 * @file C_D_TriMeshCollider.h
 * @brief 三角网格碰撞体组件（非 POD，仅用于静态体）
 *
 * @details
 * 存储静态三角网格碰撞体的顶点坐标和索引，供 Sys_Physics 构建 Jolt MeshShape。
 * 通常由 NavMesh 边界三角形数据填充，用于为多层地图的斜坡与上层平台提供物理支撑。
 *
 * 使用约束：
 * - 此组件的实体 **必须** 同时挂载 `C_D_RigidBody`（is_static = true）
 * - 此组件的实体 **不得** 同时挂载 `C_D_Collider`（Sys_Physics 对二者的处理路径不同）
 * - 顶点坐标应为 Jolt 体局部空间（调用方负责将世界偏移存入 C_D_Transform.position）
 */
#pragma once

#include <vector>
#include "Vector.h"

struct C_D_TriMeshCollider {
    std::vector<NCL::Maths::Vector3> vertices;  ///< 顶点坐标（体局部空间）
    std::vector<int>                 indices;   ///< 三角形索引，每 3 个为一个三角形

    float friction    = 0.5f;
    float restitution = 0.0f;
};
