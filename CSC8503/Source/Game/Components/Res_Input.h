/**
 * @file Res_Input.h
 * @brief 全局输入资源：存储鼠标、键盘、手柄的输入状态快照
 *
 * @details
 * `Res_Input` 是所有需要响应玩家输入的 System 的数据源，由 `InputAdapter` 在每帧开始时从 NCL 输入设备同步。
 *
 * ## 输入处理流程
 *
 * ```
 * Main Loop
 *   ├─ InputAdapter::Update(NCL::Window*, Res_Input&)  // 读取 NCL 输入设备
 *   │    └─ 同步鼠标位置、键盘状态、手柄轴值到 Res_Input
 *   ├─ Sys_PlayerController::OnUpdate(Registry&, dt)   // 读取 Res_Input
 *   │    └─ 处理玩家移动/跳跃/射击逻辑
 *   └─ Sys_UI::OnUpdate(Registry&, dt)                 // 读取 Res_Input
 *        └─ 处理菜单导航、按钮点击
 * ```
 *
 * ## 输入模式
 *
 * ### 1. 离散输入（Discrete Input）
 *
 * 使用 `keyStates` 数组直接查询按键状态：
 * - `keyStates[KeyCodes::W]` → 当前帧 W 键是否按下
 * - 适用场景：实时移动、连续射击
 *
 * ### 2. 轴输入（Axis Input）
 *
 * 使用 `axisX` / `axisY` 获取归一化的方向输入（-1.0 ~ 1.0）：
 * - `axisX`：水平轴（A = -1，D = +1，不按 = 0）
 * - `axisY`：垂直轴（S = -1，W = +1，不按 = 0）
 * - 适用场景：角色移动、相机旋转
 *
 * ## 数据字段
 *
 * | 字段 | 类型 | 说明 |
 * |------|------|------|
 * | `mousePos` | `NCL::Maths::Vector2` | 鼠标窗口坐标（像素） |
 * | `mouseDelta` | `NCL::Maths::Vector2` | 鼠标移动增量（本帧 - 上帧） |
 * | `keyStates` | `bool[256]` | 键盘按键状态数组（true = 按下） |
 * | `axisX` | `float` | 水平轴输入（-1.0 ~ 1.0） |
 * | `axisY` | `float` | 垂直轴输入（-1.0 ~ 1.0） |
 *
 * ## 维护责任
 *
 * - **写入者**：`InputAdapter::Update()`（每帧开始时从 NCL 同步）
 * - **读取者**：`Sys_PlayerController`、`Sys_Camera`、`Sys_UI` 等
 *
 * ## 使用示例
 *
 * @code
 * // Sys_PlayerController::OnUpdate
 * void OnUpdate(Registry& reg, float dt) {
 *     auto& input = reg.ctx<Res_Input>();
 *
 *     // 方法 1：轴输入（推荐用于移动）
 *     float moveX = input.axisX;
 *     float moveY = input.axisY;
 *     Vector3 movement(moveX, 0, moveY);
 *     movement.Normalise();
 *     player_transform.position += movement * speed * dt;
 *
 *     // 方法 2：离散输入（适用于跳跃）
 *     if (input.keyStates[KeyCodes::SPACE]) {
 *         player_rigidbody.velocity.y = jumpForce;
 *     }
 * }
 * @endcode
 *
 * ## 性能考量
 *
 * - **内存占用**：272 字节（全局单例）
 * - **访问开销**：O(1) 数组查询
 * - **帧同步**：确保所有 System 读取到的是同一帧的输入状态
 *
 * @note `keyStates` 数组大小固定为 256（覆盖所有 ASCII 码 + 扩展按键）。
 *
 * @see InputAdapter (更新 Res_Input 的桥接层)
 * @see NCL::KeyCodes (按键枚举定义)
 */

#pragma once

#include "Vector.h"

namespace ECS {

/**
 * @brief 全局输入资源：鼠标、键盘、手柄的输入状态快照。
 *
 * @details
 * 由 `InputAdapter` 在每帧开始时更新，所有 System 只读访问。
 */
struct Res_Input {
    // ── 鼠标 ──
    NCL::Maths::Vector2 mousePos{0.0f, 0.0f};   ///< 鼠标窗口坐标（像素）
    NCL::Maths::Vector2 mouseDelta{0.0f, 0.0f}; ///< 鼠标移动增量（本帧 - 上帧）
    bool mouseButtons[5] = {};                    ///< 鼠标按钮持续状态（ButtonDown）
    bool mouseButtonPressed[5] = {};              ///< 鼠标按钮边沿（本帧刚按下）

    // ── 键盘 ──
    bool keyStates[256] = {};    ///< 键盘按键持续状态（KeyDown）
    bool keyPressed[256] = {};   ///< 键盘按键边沿（本帧刚按下）

    // ── 合成轴 ──
    float axisX = 0.0f; ///< 水平轴输入（A/左 = -1，D/右 = +1，不按 = 0）
    float axisY = 0.0f; ///< 垂直轴输入（S/下 = -1，W/上 = +1，不按 = 0）

    // ── 系统事件 ──
    bool quitRequested = false; ///< Alt+F4 退出请求
};

} // namespace ECS
