/**
 * @file C_D_DataOceanPillar.h
 * @brief 数据海洋柱子组件：存储每个柱子的噪波动画参数。
 *
 * @details
 * `C_D_DataOceanPillar` 为数据海洋效果中的每个柱子实体提供个体化噪波参数，
 * 由 `Sys_DataOcean` 在 OnUpdate 中读取并驱动柱子的 Y 轴起伏动画。
 *
 * POD struct, 16 bytes.
 */
#pragma once

namespace ECS {

/**
 * @brief 数据海洋柱子噪波参数组件。
 *
 * @details
 * 每个柱子持有独立的基准 Y、振幅、相位偏移和 XZ 世界尺寸，
 * Sys_DataOcean 结合全局噪波配置计算最终 Y 位置。
 */
struct C_D_DataOceanPillar {
    float baseY      = 0.0f; ///< 初始 Y 位置（由生成时写入）
    float amplitude  = 2.0f; ///< 个体振幅倍率
    float phaseShift = 0.0f; ///< 噪波相位偏移（基于 XZ 坐标哈希）
    float sizeXZ     = 4.0f; ///< 柱子 XZ 世界尺寸（递归细分决定）
};

} // namespace ECS
