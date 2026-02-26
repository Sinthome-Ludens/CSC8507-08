/**
 * @file InputAdapter.cpp
 * @brief InputAdapter 实现：从 NCL 输入同步到 Res_Input
 */

#include "InputAdapter.h"
#include "Keyboard.h"
#include "Game/Utils/Assert.h"

using namespace NCL;

namespace ECS {

void InputAdapter::Update(Window* window, Res_Input& input) {
    GAME_ASSERT(window != nullptr, "InputAdapter::Update - window is null");

    auto* keyboard = window->GetKeyboard();
    auto* mouse = window->GetMouse();
    if (!keyboard || !mouse) return;  // 防御性空指针检查

    // 同步鼠标位置
    input.mousePos = mouse->GetAbsolutePosition();
    input.mouseDelta = mouse->GetRelativePosition();

    // 同步键盘状态（遍历所有按键）
    for (int i = 0; i < 256; ++i) {
        input.keyStates[i] = keyboard->KeyDown((KeyCodes::Type)i);
    }

    // 合成轴输入（WASD）
    float axisX = 0.0f;
    float axisY = 0.0f;

    if (input.keyStates[KeyCodes::A]) axisX -= 1.0f; // 向左
    if (input.keyStates[KeyCodes::D]) axisX += 1.0f; // 向右
    if (input.keyStates[KeyCodes::S]) axisY -= 1.0f; // 向后/下
    if (input.keyStates[KeyCodes::W]) axisY += 1.0f; // 向前/上

    input.axisX = axisX;
    input.axisY = axisY;
}

} // namespace ECS
