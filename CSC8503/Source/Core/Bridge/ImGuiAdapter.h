#pragma once
#ifdef USE_IMGUI

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_opengl3.h>
#include <windows.h>

namespace NCL {
    class Window;
    namespace Rendering { class OGLRenderer; }
}

namespace ECS {

class ImGuiAdapter {
public:
    static bool Init(NCL::Window* window, NCL::Rendering::OGLRenderer* renderer);
    static void Shutdown();
    static void NewFrame();
    static void Render();
    static bool IsCapturingInput();

private:
    static HWND GetHWND(NCL::Window* window);

    static bool  s_Initialized;
    static HWND  s_TargetHWND;
    static NCL::Window* s_Window;

    friend LRESULT CALLBACK ImGuiSubclassProc(
        HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
};

} // namespace ECS
#endif
