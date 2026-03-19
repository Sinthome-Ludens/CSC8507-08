/**
 * @file Sys_DataOcean.h
 * @brief 数据海洋系统：分帧批量生成柱子并驱动噪波起伏动画。
 *
 * @details
 * `Sys_DataOcean` 在 OnAwake 中通过递归细分算法收集所有柱子的生成参数，
 * 并通过 PrefabFactory::ResolveBlueprintCache 预缓存组件函数指针。
 * 在 OnUpdate 中分帧创建实体（每帧 spawnBatchSize 个），
 * 创建完毕后切换到噪波动画更新模式。
 *
 * 执行优先级：195（在 Sys_Render=200 之前，确保 Transform 已更新）
 *
 * 依赖资源：
 *   - `Res_DataOcean`（海洋配置参数 + 分帧创建状态，场景 OnEnter 中 ctx_emplace）
 *   - `Res_Time`（全局时间，主循环维护）
 *
 * 性能优化：
 *   - Resolve-Once：JSON 解析和 ComponentRegistry 查表只在 OnAwake 执行一次
 *   - Time-Sliced Spawning：实体创建分散到多帧，消除单帧卡顿
 *   - 所有柱子共享同一 MeshHandle，渲染器可批量绘制
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 数据海洋系统：分帧批量生成柱子并驱动噪波动画。
 */
class Sys_DataOcean : public ISystem {
public:
    Sys_DataOcean() = default;

    /// @brief 递归细分收集生成参数，预缓存组件函数指针，启动分帧创建。
    void OnAwake(Registry& registry) override;

    /// @brief 分帧创建实体（spawning 阶段）或更新噪波动画（正常阶段）。
    void OnUpdate(Registry& registry, float dt) override;

    /// @brief 无需特殊清理。
    void OnDestroy(Registry& registry) override;
};

} // namespace ECS
