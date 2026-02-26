#include "ImGuiAdapter.h"
#ifdef USE_IMGUI

#include "Window.h"
#include "Win32Window.h"
#include "OGLRenderer.h"
#include "Game/Utils/Log.h"

using namespace NCL;
using namespace NCL::Win32Code;

namespace ECS {

bool ImGuiAdapter::s_Initialized = false;
HHOOK ImGuiAdapter::s_MessageHook = nullptr;
HWND  ImGuiAdapter::s_TargetHWND  = nullptr;

LRESULT CALLBACK ImGuiAdapter::MessageHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_Initialized) {
        MSG* pMsg = reinterpret_cast<MSG*>(lParam);
        if (pMsg->hwnd == s_TargetHWND) {
            extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
            ImGui_ImplWin32_WndProcHandler(pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam);
        }
    }
    return CallNextHookEx(s_MessageHook, nCode, wParam, lParam);
}

HWND ImGuiAdapter::GetHWND(Window* window) {
    Win32Window* win32Window = dynamic_cast<Win32Window*>(window);
    return win32Window ? win32Window->GetHandle() : nullptr;
}

bool ImGuiAdapter::Init(Window* window, Rendering::OGLRenderer* renderer) {
    if (s_Initialized) return false;

    s_TargetHWND = GetHWND(window);
    if (!s_TargetHWND) {
        LOG_ERROR("[ImGuiAdapter] Failed to get HWND from Window");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // ViewportsEnable 不适用于单窗口游戏（会切换 WGL context 导致渲染问题）
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(s_TargetHWND)) {
        LOG_ERROR("[ImGuiAdapter] ImGui_ImplWin32_Init failed");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        LOG_ERROR("[ImGuiAdapter] ImGui_ImplOpenGL3_Init failed");
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    DWORD threadId = GetWindowThreadProcessId(s_TargetHWND, nullptr);
    s_MessageHook = SetWindowsHookEx(WH_GETMESSAGE, MessageHookProc, nullptr, threadId);

    if (!s_MessageHook) {
        LOG_ERROR("[ImGuiAdapter] SetWindowsHookEx failed (error: " << GetLastError() << ")");
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    s_Initialized = true;
    LOG_INFO("[ImGuiAdapter] Initialized (No-Intrusive Mode, Docking enabled)");
    return true;
}

void ImGuiAdapter::Shutdown() {
    if (!s_Initialized) return;
    if (s_MessageHook) {
        UnhookWindowsHookEx(s_MessageHook);
        s_MessageHook = nullptr;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_Initialized = false;
    LOG_INFO("[ImGuiAdapter] Shutdown complete");
}

void ImGuiAdapter::NewFrame() {
    if (!s_Initialized) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiAdapter::Render() {
    if (!s_Initialized) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
