/**
 * @file Res_DataOcean.h
 * @brief 数据海洋全局资源：海洋效果的配置参数与分帧创建状态。
 *
 * @details
 * `Res_DataOcean` 存储数据海洋效果的全局配置，由场景在 OnEnter 中
 * `ctx_emplace` 注册，Sys_DataOcean 在 OnAwake/OnUpdate 中读取。
 *
 * 分帧创建（Time-Sliced Spawning）状态也存储在此资源中，
 * OnAwake 收集所有待创建实体的 RuntimeOverrides 到 pendingSpawns，
 * OnUpdate 每帧消费 spawnBatchSize 个，避免单帧卡顿。
 *
 * 场景 OnExit 时须手动 `ctx_erase<Res_DataOcean>()`。
 */
#pragma once

#include "Game/Prefabs/RuntimeOverrides.h"
#include "Core/ECS/Registry.h"
#include <nlohmann/json_fwd.hpp>
#include <vector>
#include <functional>

namespace ECS {

/// @brief 缓存的组件 Emplace 函数指针与对应 JSON 数据指针。
struct CachedEmplaceEntry {
    std::function<void(Registry&, EntityID, const nlohmann::json&, const RuntimeOverrides&)> fn;
    const nlohmann::json* jsonData = nullptr;
};

/**
 * @brief 数据海洋全局配置资源。
 *
 * @details
 * 使用递归细分布局：areaExtent 定义海洋半径，baseSize 为递归起点格子尺寸，
 * maxDepth 控制最大细分深度（最小方块 = baseSize / 2^maxDepth）。
 *
 * 分帧创建字段（spawn* / pending*）由 Sys_DataOcean 内部管理，
 * 场景配置时无需设置。
 */
struct Res_DataOcean {
    // ── 海洋配置参数 ─────────────────────────────────────
    float areaExtent      = 100.0f; ///< 海洋半径（世界单位）
    float baseSize        = 4.0f;   ///< 大格子尺寸（递归起点）
    float minSize         = 0.5f;   ///< 最小方块尺寸（不再细分）
    int   maxDepth        = 2;      ///< 最大递归深度
    float splitChanceBase = 0.35f;  ///< depth=0 时的细分概率
    float centerGap       = 5.0f;   ///< 中心空洞半径
    float noiseScale      = 0.05f;  ///< 噪波空间频率
    float noiseSpeed      = 0.3f;   ///< 噪波时间滚动速度
    float baseAmplitude   = 3.0f;   ///< 全局振幅倍率
    float pillarMinScaleY = 4.0f;   ///< 柱子 Y 缩放下限（防露缝）
    float pillarMaxScaleY = 8.0f;   ///< 柱子 Y 缩放上限
    float yOffset         = -20.0f; ///< 整体 Y 偏移

    // ── 噪波更新频率控制 ───────────────────────────────────
    int   noiseUpdateInterval = 4;  ///< 每 N 帧更新一次噪波（降低 CPU 开销）
    int   noiseFrameCounter   = 0;  ///< 噪波帧计数器（Sys_DataOcean 内部管理）

    // ── GPU 驱动噪波时间（由 Sys_DataOcean 每帧写入，渲染器读取）──
    float gpuTime         = 0.0f;   ///< 传递给 vertex shader 的 oceanTime uniform

    // ── 加载状态（由 Sys_Render 写入，Sys_UI 读取判断 loading screen 是否可关闭）──
    bool  allProxiesCreated = false; ///< 所有柱子 proxy 已创建完毕
    int   totalSpawnCount   = 0;    ///< spawning 开始时记录的总柱子数（pendingSpawns 会被清空，此值不变）

    // ── 分帧创建参数 ─────────────────────────────────────
    int   spawnBatchSize  = 500;    ///< 每帧创建实体数

    // ── 分帧创建运行时状态（Sys_DataOcean 内部管理）──────
    std::vector<RuntimeOverrides>    pendingSpawns;  ///< 待创建实体的覆盖参数
    std::vector<CachedEmplaceEntry>  cachedEmplace;  ///< Resolve-Once 缓存的函数指针
    int  spawnCursor      = 0;      ///< 当前已创建到的索引
    bool spawning         = false;  ///< 是否正在分帧创建中
};

} // namespace ECS
