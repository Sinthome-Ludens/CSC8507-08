/**
 * @file Res_Time.h
 * @brief 全局时间资源：存储帧时间、物理步长、时间缩放等全局时间状态
 *
 * @details
 * `Res_Time` 是所有需要时间信息的 System 的数据源，由主循环（Main Loop）在每帧开始时更新。
 *
 * ## 时间系统架构
 *
 * 游戏主循环维护两种时间步长：
 *
 * 1. **变长时间步（Variable Timestep）**：
 *    - 字段：`deltaTime`
 *    - 用途：渲染插值、UI 动画、输入响应
 *    - 特点：随帧率波动，60 FPS 时约 0.016s
 *
 * 2. **定长时间步（Fixed Timestep）**：
 *    - 字段：`fixedDeltaTime`
 *    - 用途：物理模拟、网络同步、AI 决策
 *    - 特点：固定值（默认 1/60 秒），确保确定性行为
 *
 * ## 时间缩放机制
 *
 * `timeScale` 用于实现慢动作/快进效果，影响所有时间相关的逻辑：
 *
 * - `timeScale = 1.0`：正常速度
 * - `timeScale = 0.5`：慢动作（子弹时间）
 * - `timeScale = 2.0`：快进
 * - `timeScale = 0.0`：完全暂停（等价于 `isPaused = true`）
 *
 * @note `timeScale` 不影响 UI 动画和输入响应，仅影响游戏逻辑。
 *
 * ## 数据字段
 *
 * | 字段 | 类型 | 说明 |
 * |------|------|------|
 * | `deltaTime` | `float` | 上一帧耗时（秒），变长步长 |
 * | `fixedDeltaTime` | `float` | 物理步长（秒），固定值（默认 0.01666） |
 * | `timeScale` | `float` | 时间缩放因子（1.0 = 正常速度） |
 * | `totalTime` | `float` | 游戏运行总时长（秒），不受暂停影响 |
 * | `frameCount` | `uint64_t` | 总帧数计数器 |
 *
 * ## 维护责任
 *
 * - **写入者**：`Main::GameLoop()`（主循环在每帧开始时更新）
 * - **读取者**：所有需要时间信息的 System
 *
 * ## 使用示例
 *
 * @code
 * // Sys_Movement::OnUpdate
 * void OnUpdate(Registry& reg, float dt) {
 *     auto& time = reg.ctx<Res_Time>();
 *     float scaledDt = dt * time.timeScale; // 应用时间缩放
 *
 *     reg.view<C_D_Transform, C_D_Velocity>().each([scaledDt](auto, auto& tf, auto& vel) {
 *         tf.position += vel.velocity * scaledDt;
 *     });
 * }
 * @endcode
 *
 * ## 性能考量
 *
 * - **内存占用**：20 字节（全局单例，无性能压力）
 * - **访问开销**：O(1) 通过 `registry.ctx<Res_Time>()` 获取引用
 *
 * @see Main.cpp (更新 Res_Time 的主循环)
 * @see Sys_Physics (使用 fixedDeltaTime)
 */

#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 全局时间资源：帧时间、物理步长、时间缩放。
 *
 * @details
 * 由主循环在每帧开始时更新，所有 System 只读访问。
 */
struct Res_Time {
    float    deltaTime      = 0.0f;        ///< 上一帧耗时（秒），变长步长
    float    fixedDeltaTime = 1.0f / 60.0f; ///< 物理步长（秒），固定值（默认 1/60）
    float    timeScale      = 1.0f;        ///< 时间缩放因子（1.0 = 正常速度）
    float    totalTime      = 0.0f;        ///< 游戏运行总时长（秒）
    uint64_t frameCount     = 0;           ///< 总帧数计数器
};

} // namespace ECS
