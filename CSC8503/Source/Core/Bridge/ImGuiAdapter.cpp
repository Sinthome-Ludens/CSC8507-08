#include "ImGuiAdapter.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Win32Window.h"
#include "Mouse.h"
#include "OGLRenderer.h"
#include "glad/gl.h"
#include "Game/Utils/WindowHelper.h"
#include "Game/Utils/Log.h"
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <iostream>

using namespace NCL;
using namespace NCL::Win32Code;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND, UINT, WPARAM, LPARAM);

namespace ECS {

bool ImGuiAdapter::s_Initialized = false;
HWND  ImGuiAdapter::s_TargetHWND  = nullptr;
NCL::Window* ImGuiAdapter::s_Window = nullptr;

static const UINT_PTR kImGuiSubclassId = 2; // WindowHelper uses 1

static LRESULT CALLBACK ImGuiSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    // subclass 仍然转发非鼠标消息（WM_CHAR、WM_MOUSEWHEEL 等）给 ImGui
    if (ImGuiAdapter::s_Initialized && !WindowHelper::IsInSizeMove()) {
        ::ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

HWND ImGuiAdapter::GetHWND(Window* window) {
    Win32Window* win32Window = dynamic_cast<Win32Window*>(window);
    return win32Window ? win32Window->GetHandle() : nullptr;
}

bool ImGuiAdapter::Init(Window* window, Rendering::OGLRenderer* renderer) {
    if (s_Initialized) return false;

    s_TargetHWND = GetHWND(window);
    if (!s_TargetHWND) {
        std::cerr << "[ImGuiAdapter] Failed to get HWND from Window" << std::endl;
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ViewportsEnable 不适用于单窗口游戏（会切换 WGL context 导致渲染问题）
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(s_TargetHWND)) {
        std::cerr << "[ImGuiAdapter] ImGui_ImplWin32_Init failed" << std::endl;
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "[ImGuiAdapter] ImGui_ImplOpenGL3_Init failed" << std::endl;
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    if (!SetWindowSubclass(s_TargetHWND, ImGuiSubclassProc, kImGuiSubclassId, 0)) {
        std::cerr << "[ImGuiAdapter] SetWindowSubclass failed" << std::endl;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    s_Window = window;
    s_Initialized = true;
    std::cout << "[ImGuiAdapter] Initialized (No-Intrusive Mode, Docking enabled)" << std::endl;
    return true;
}

void ImGuiAdapter::Shutdown() {
    if (!s_Initialized) return;
    RemoveWindowSubclass(s_TargetHWND, ImGuiSubclassProc, kImGuiSubclassId);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_Window = nullptr;
    s_Initialized = false;
    std::cout << "[ImGuiAdapter] Shutdown complete" << std::endl;
}

void ImGuiAdapter::NewFrame() {
    if (!s_Initialized) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();

    // 从 NCL Mouse 轮询鼠标状态，绕过 ImGui Win32 后端的 MouseTrackedArea 守卫。
    // 当 WM_MOUSEMOVE 消息不可靠到达时（场景切换后间歇性发生），
    // MouseTrackedArea 卡在非零值，阻止 GetCursorPos 回退，导致鼠标位置冻结。
    // 通过 NCL Mouse 轮询保证每帧都有正确的鼠标数据，且与游戏输入源统一。
    {
        ImGuiIO& io = ImGui::GetIO();
        if (s_Window) {
            const auto* mouse = s_Window->GetMouse();
            if (mouse) {
                Maths::Vector2 pos = mouse->GetAbsolutePosition();
                io.AddMousePosEvent(pos.x, pos.y);
                io.AddMouseButtonEvent(0, mouse->ButtonDown(MouseButtons::Left));
                io.AddMouseButtonEvent(1, mouse->ButtonDown(MouseButtons::Right));
                io.AddMouseButtonEvent(2, mouse->ButtonDown(MouseButtons::Middle));
            }
        }
    }

    ImGui::NewFrame();
}

void ImGuiAdapter::Render() {
    if (!s_Initialized) return;

    // 关闭 sRGB 帧缓冲转换：ImGui 颜色已是 sRGB 空间，
    // 若不关闭会被 GL 再次 gamma 编码导致所有 UI 颜色偏亮偏白。
    GLboolean srgbWasEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    if (srgbWasEnabled) glDisable(GL_FRAMEBUFFER_SRGB);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 恢复 sRGB 状态（3D 渲染需要）
    if (srgbWasEnabled) glEnable(GL_FRAMEBUFFER_SRGB);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

bool ImGuiAdapter::IsCapturingInput() {
    if (!s_Initialized) return false;
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}

} // namespace ECS
#endif
