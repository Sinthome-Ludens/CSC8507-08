/**
 * @file InputAdapter.cpp
 * @brief InputAdapter 实现：从 NCL 输入同步到 Res_Input
 */

#include "InputAdapter.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Game/Utils/Assert.h"

using namespace NCL;

namespace ECS {

void InputAdapter::Update(Window* window, Res_Input& input) {
    GAME_ASSERT(window != nullptr, "InputAdapter::Update - window is null");

    auto* kb    = window->GetKeyboard();
    auto* mouse = window->GetMouse();

    // ── 键盘：边沿检测后覆写 ──
    for (int i = 0; i < 256; ++i) {
        bool now = kb->KeyDown((KeyCodes::Type)i);
        input.keyPressed[i] = now && !input.keyStates[i];   // 上升沿
        input.keyStates[i]  = now;
    }

    // ── 鼠标按钮：边沿检测后覆写 ──
    for (int i = 0; i < MouseButtons::MAX_VAL; ++i) {
        bool now = mouse->ButtonDown((MouseButtons::Type)i);
        input.mouseButtonPressed[i] = now && !input.mouseButtons[i];
        input.mouseButtons[i]       = now;
    }

    // ── 鼠标位置 & 滚轮 ──
    input.mousePos    = mouse->GetAbsolutePosition();
    input.mouseDelta  = mouse->GetRelativePosition();
    input.scrollWheel = mouse->GetWheelMovement();

    // ── 合成轴 ──
    input.axisX = (input.keyStates[KeyCodes::D] ? 1.f : 0.f)
                - (input.keyStates[KeyCodes::A] ? 1.f : 0.f);
    input.axisY = (input.keyStates[KeyCodes::W] ? 1.f : 0.f)
                - (input.keyStates[KeyCodes::S] ? 1.f : 0.f);

    // ── 系统事件 ──
    // Alt+F4 退出请求（通过 Res_Input 传递给 Main 循环）
    input.quitRequested = input.keyStates[KeyCodes::MENU]
                       && input.keyPressed[KeyCodes::F4];
}

} // namespace ECS
