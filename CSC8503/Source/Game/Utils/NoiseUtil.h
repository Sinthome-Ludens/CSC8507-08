/**
 * @file NoiseUtil.h
 * @brief 纯头文件噪波工具：hash-based gradient noise，返回 [-1, 1]。
 *
 * @details
 * 提供无状态、无依赖、无动态分配的 3D 梯度噪波函数，
 * 供 Sys_DataOcean 等系统驱动程序化动画。
 */
#pragma once

#include <cmath>
#include <cstdint>

namespace ECS {

namespace NoiseUtil {

/**
 * @brief 整数哈希（基于 Hugo Elias 风格位运算）
 */
inline float HashFloat(int x, int y, int z) {
    int n = x * 73856093 ^ y * 19349663 ^ z * 83492791;
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return static_cast<float>(n & 0x7fffffff) / 1073741824.0f - 1.0f;
}

/**
 * @brief 平滑插值（Hermite 曲线 3t² - 2t³）
 */
inline float Fade(float t) {
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief 线性插值
 */
inline float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

/**
 * @brief 3D 值噪波，返回 [-1, 1]。
 *
 * 基于整数格点哈希 + 三线性插值，无查找表依赖。
 * 适用于程序化动画（柱子起伏、水面波动等）。
 *
 * @param x X 坐标（连续浮点）
 * @param y Y 坐标（连续浮点）
 * @param z Z 坐标（连续浮点）
 * @return 噪波值 [-1, 1]
 */
inline float Noise3D(float x, float y, float z) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    int iz = static_cast<int>(std::floor(z));

    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);
    float fz = z - static_cast<float>(iz);

    float u = Fade(fx);
    float v = Fade(fy);
    float w = Fade(fz);

    float c000 = HashFloat(ix,     iy,     iz);
    float c100 = HashFloat(ix + 1, iy,     iz);
    float c010 = HashFloat(ix,     iy + 1, iz);
    float c110 = HashFloat(ix + 1, iy + 1, iz);
    float c001 = HashFloat(ix,     iy,     iz + 1);
    float c101 = HashFloat(ix + 1, iy,     iz + 1);
    float c011 = HashFloat(ix,     iy + 1, iz + 1);
    float c111 = HashFloat(ix + 1, iy + 1, iz + 1);

    float x00 = Lerp(c000, c100, u);
    float x10 = Lerp(c010, c110, u);
    float x01 = Lerp(c001, c101, u);
    float x11 = Lerp(c011, c111, u);

    float y0 = Lerp(x00, x10, v);
    float y1 = Lerp(x01, x11, v);

    return Lerp(y0, y1, w);
}

} // namespace NoiseUtil

} // namespace ECS
