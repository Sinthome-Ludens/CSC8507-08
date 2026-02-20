/**
 * @file InputAdapter.h
 * @brief 输入适配器：从 NCL 输入设备同步到 ECS 全局资源 Res_Input
 *
 * @details
 * `InputAdapter` 是 Bridge 层的核心组件之一，负责将 NCL 的 OOP 输入接口转换为 ECS 的数据驱动输入模型。
 *
 * ## 设计模式
 *
 * **适配器模式（Adapter Pattern）**：
 * - **Source**：NCL::Window::GetKeyboard() / GetMouse()
 * - **Target**：ECS::Res_Input
 * - **Adapter**：InputAdapter::Update()
 *
 * ## 数据流
 *
 * ```
 * NCL::Window
 *   ├─ GetKeyboard()->KeyDown(KeyCodes::W)  → Res_Input::keyStates[KeyCodes::W]
 *   ├─ GetMouse()->GetAbsolutePosition()    → Res_Input::mousePos
 *   └─ GetMouse()->GetRelativePosition()    → Res_Input::mouseDelta
 *
 * InputAdapter::Update(window, input)
 *   └─ 合成轴输入
 *       ├─ WASD → axisX (-1.0 ~ 1.0)
 *       └─ WASD → axisY (-1.0 ~ 1.0)
 * ```
 *
 * ## 轴输入合成规则
 *
 * ### 水平轴（axisX）
 * - **A** 键按下：-1.0（向左）
 * - **D** 键按下：+1.0（向右）
 * - **A + D** 同时按下：0.0（抵消）
 * - 均未按下：0.0（静止）
 *
 * ### 垂直轴（axisY）
 * - **S** 键按下：-1.0（向后/下）
 * - **W** 键按下：+1.0（向前/上）
 * - **W + S** 同时按下：0.0（抵消）
 * - 均未按下：0.0（静止）
 *
 * ## 使用约束
 *
 * 1. **无状态设计**：InputAdapter 不存储任何状态，所有数据直接写入 Res_Input。
 * 2. **每帧一次调用**：在主循环的 **帧开始** 位置调用 `Update()`，确保所有 System 读取到同步的输入。
 * 3. **只读 NCL**：InputAdapter 仅读取 NCL 输入设备，不修改任何 NCL 状态。
 *
 * ## 主循环集成
 *
 * @code
 * while (window->UpdateWindow()) {
 *     float dt = timer.GetDeltaTime();
 *
 *     // 1. 更新输入（帧开始）
 *     InputAdapter::Update(window, registry.ctx<Res_Input>());
 *
 *     // 2. 更新时间
 *     auto& time = registry.ctx<Res_Time>();
 *     time.deltaTime = dt;
 *     time.frameCount++;
 *
 *     // 3. 执行 System 逻辑
 *     systemManager.UpdateAll(registry, dt);
 *
 *     // ...
 * }
 * @endcode
 *
 * ## 性能考量
 *
 * - **调用开销**：O(256)，遍历所有按键状态
 * - **优化机会**：可改为仅同步"脏"按键（需 NCL 支持事件机制）
 *
 * @note InputAdapter 是静态工具类，无需实例化。
 *
 * @see Res_Input (目标数据结构)
 * @see NCL::Window (输入数据源)
 */

#pragma once

#include "Game/Components/Res_Input.h"
#include "Window.h"
#include "Keyboard.h"
#include "Mouse.h"

namespace ECS {

/**
 * @brief 输入适配器：从 NCL 输入设备同步到 Res_Input（静态工具类）。
 *
 * @details
 * 无状态设计，所有方法均为静态函数，在主循环中每帧调用一次 `Update()`。
 */
class InputAdapter {
public:
    /**
     * @brief 从 NCL 输入设备同步到 Res_Input。
     * @details 应在主循环的 **帧开始** 位置调用，确保所有 System 读取到同步的输入。
     * @param window NCL 窗口对象指针（非空）
     * @param input  目标 Res_Input 引用（将被写入）
     */
    static void Update(NCL::Window* window, Res_Input& input);

private:
    InputAdapter() = delete; // 静态工具类，禁止实例化
};

} // namespace ECS
