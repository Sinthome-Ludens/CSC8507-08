#include "ImGuiAdapter.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Win32Window.h"
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

static const UINT_PTR kImGuiSubclassId = 2; // WindowHelper uses 1

static LRESULT CALLBACK ImGuiSubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    // 诊断 A：subclass proc 是否被调用（仅鼠标消息，避免刷屏）
    if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_MOUSEMOVE) {
        if (uMsg != WM_MOUSEMOVE) { // MOUSEMOVE 太频繁，只记录点击
            LOG_INFO("[ImGuiSubclass-A] CALLED msg=" << uMsg
                     << " init=" << ImGuiAdapter::s_Initialized
                     << " sizeMove=" << WindowHelper::IsInSizeMove()
                     << " hWnd=" << hWnd);
        }
    }

    if (ImGuiAdapter::s_Initialized && !WindowHelper::IsInSizeMove()) {
        // 诊断 B：转发到 ImGui
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) {
            LOG_INFO("[ImGuiSubclass-B] FORWARDING msg=" << uMsg << " to ImGui");
        }

        LRESULT result = ::ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

        // 诊断 C：ImGui 是否消费了消息
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) {
            ImGuiIO& io = ImGui::GetIO();
            LOG_INFO("[ImGuiSubclass-C] WndProcHandler returned=" << result
                     << " WantCaptureMouse=" << io.WantCaptureMouse
                     << " MouseDown[0]=" << io.MouseDown[0]);
        }
    } else {
        // 诊断 D：为什么没有转发
        if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP) {
            LOG_INFO("[ImGuiSubclass-D] BLOCKED init=" << ImGuiAdapter::s_Initialized
                     << " sizeMove=" << WindowHelper::IsInSizeMove());
        }
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
    s_Initialized = false;
    std::cout << "[ImGuiAdapter] Shutdown complete" << std::endl;
}

void ImGuiAdapter::NewFrame() {
    if (!s_Initialized) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 诊断 E：每 300 帧打印一次状态，确认 ImGui 上下文存活
    static int frameCount = 0;
    if (++frameCount % 300 == 0) {
        ImGuiIO& io = ImGui::GetIO();
        LOG_INFO("[ImGuiAdapter-E] frame=" << frameCount
                 << " mousePos=(" << io.MousePos.x << "," << io.MousePos.y << ")"
                 << " wantMouse=" << io.WantCaptureMouse
                 << " ctx=" << ImGui::GetCurrentContext());
    }
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
