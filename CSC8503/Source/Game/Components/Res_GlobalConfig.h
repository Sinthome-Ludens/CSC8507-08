/**
 * @file Res_GlobalConfig.h
 * @brief 全局配置资源：存储游戏运行时的全局参数（重力、分辨率、音量等）
 *
 * @details
 * `Res_GlobalConfig` 存储游戏的全局配置参数，由场景加载器在 `OnEnter` 时从 JSON 配置文件读取。
 *
 * ## 配置来源
 *
 * - **源文件**：`Assets/Configs/GameConfig.json`
 * - **加载时机**：`Scene::OnEnter()`
 * - **修改时机**：设置菜单（Settings Menu）修改后立即生效
 *
 * ## 数据字段
 *
 * | 字段 | 类型 | 说明 |
 * |------|------|------|
 * | `gravity` | `NCL::Maths::Vector3` | 重力加速度（m/s²，默认 (0, -9.8, 0)） |
 * | `screenWidth` | `uint32_t` | 窗口宽度（像素） |
 * | `screenHeight` | `uint32_t` | 窗口高度（像素） |
 * | `masterVolume` | `float` | 主音量（0.0 ~ 1.0） |
 * | `fovDegrees` | `float` | 相机视场角（度，默认 60.0） |
 *
 * ## 使用示例
 *
 * @code
 * // Sys_Physics::OnAwake
 * void OnAwake(Registry& reg) {
 *     auto& config = reg.ctx<Res_GlobalConfig>();
 *     jolt_system->SetGravity(config.gravity); // 应用重力配置
 * }
 * @endcode
 *
 * @see Assets/Configs/GameConfig.json (配置文件源)
 */

#pragma once

#include "Vector.h"
#include <cstdint>

namespace ECS {

/**
 * @brief 全局配置资源：游戏运行时的全局参数。
 */
struct Res_GlobalConfig {
    NCL::Maths::Vector3 gravity{0.0f, -9.8f, 0.0f}; ///< 重力加速度（m/s²）
    uint32_t screenWidth  = 1280;                   ///< 窗口宽度（像素）
    uint32_t screenHeight = 720;                    ///< 窗口高度（像素）
    float masterVolume    = 1.0f;                   ///< 主音量（0.0 ~ 1.0）
    float fovDegrees      = 60.0f;                  ///< 相机视场角（度）
};

} // namespace ECS
