/**
 * @file Res_MinimapState.h
 * @brief 小地图全局状态资源：缓存 NavMesh 边界线段及激活标志。
 *
 * @details
 * 场景级 ctx 资源，由 Scene OnEnter 写入边界缓存，
 * Sys_ItemEffects::EffectRadarMap 激活，UI_HUD 读取渲染。
 *
 * @see Sys_ItemEffects.h
 * @see UI_HUD.cpp
 */
#pragma once
#include <cstdint>

namespace ECS {

/// @brief 小地图边界线段（NavMesh 世界坐标 XZ 投影）
struct MinimapEdge {
    float x0, z0;  ///< 起点 XZ
    float x1, z1;  ///< 终点 XZ
};

/// @brief 小地图可行走三角形（NavMesh XZ 投影，用于填充绘制）
struct MinimapTriangle {
    float x0, z0;
    float x1, z1;
    float x2, z2;
};

/// @brief 小地图全局状态资源（Scene ctx）
struct Res_MinimapState {
    static constexpr int   kMaxEdges       = 512;
    static constexpr int   kMaxTriangles   = 1024;
    static constexpr float kActiveDuration = 10.0f; ///< 激活持续时间（秒）

    bool  isActive    = false; ///< 是否激活显示（道具使用后置 true）
    float activeTimer = 0.0f;  ///< 剩余显示时间（秒），到 0 时自动关闭
    int   edgeCount   = 0;     ///< 缓存的边界边数量

    MinimapEdge edges[kMaxEdges] = {};  ///< 边界边缓存

    int             triangleCount = 0;                  ///< 缓存的可行走三角形数量
    MinimapTriangle triangles[kMaxTriangles] = {};      ///< 可行走三角形缓存

    // 地图 AABB 包围盒（世界坐标，用于坐标映射）
    float worldMinX =  1e9f;
    float worldMaxX = -1e9f;
    float worldMinZ =  1e9f;
    float worldMaxZ = -1e9f;
};

} // namespace ECS
