#include "WindowHelper.h"
#include "Win32Window.h"
#include "Game/Utils/Log.h"

using NCL::Win32Code::Win32Window;

// ── File-scope state ────────────────────────────────────────
static Win32Window* s_win32      = nullptr;
static HWND         s_hwnd       = nullptr;
static bool         s_fullscreen = false;
static int          s_defaultW   = 1920;
static int          s_defaultH   = 1080;

// ── Init ────────────────────────────────────────────────────

bool WindowHelper::Init(NCL::Window* window) {
    if (!window) {
        LOG_ERROR("[WindowHelper] Init failed: window is null");
        return false;
    }
    s_win32 = dynamic_cast<Win32Window*>(window);
    if (!s_win32) {
        LOG_ERROR("[WindowHelper] Init failed: Window is not a Win32Window");
        return false;
    }
    s_hwnd = s_win32->GetHandle();

    auto size = window->GetScreenSize();
    s_defaultW = size.x;
    s_defaultH = size.y;

    LOG_INFO("[WindowHelper] Init OK. handle=" << s_hwnd
             << " defaultSize=" << s_defaultW << "x" << s_defaultH);
    return true;
}

// ── SetFullScreen ───────────────────────────────────────────

void WindowHelper::SetFullScreen(bool fullscreen) {
    if (!s_hwnd) {
        LOG_ERROR("[WindowHelper] SetFullScreen called before Init");
        return;
    }

    s_fullscreen = fullscreen;

    if (fullscreen) {
        // Borderless fullscreen: remove title bar / borders, cover entire screen
        SetWindowLongPtr(s_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        SetWindowPos(s_hwnd, HWND_TOPMOST,
                     0, 0, screenW, screenH,
                     SWP_FRAMECHANGED);
    }
    else {
        // Restore windowed mode with standard chrome
        DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        SetWindowLongPtr(s_hwnd, GWL_STYLE, style);

        // Compute window rect that yields the desired client area
        RECT rc = { 0, 0, (LONG)s_defaultW, (LONG)s_defaultH };
        AdjustWindowRectEx(&rc, style, FALSE, 0);
        int windowW = rc.right  - rc.left;
        int windowH = rc.bottom - rc.top;

        // Center on screen
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int posX = (screenW - windowW) / 2;
        int posY = (screenH - windowH) / 2;

        SetWindowPos(s_hwnd, HWND_NOTOPMOST,
                     posX, posY, windowW, windowH,
                     SWP_FRAMECHANGED);
    }

    // Trigger WindowProc's applyResize path → eventHandler (viewport)
    // + winMouse bounds + LockMouseToWindow re-apply
    SendMessage(s_hwnd, WM_EXITSIZEMOVE, 0, 0);
}

// ── SetWindowSize ───────────────────────────────────────────

void WindowHelper::SetWindowSize(int w, int h) {
    if (!s_hwnd) {
        LOG_ERROR("[WindowHelper] SetWindowSize called before Init");
        return;
    }
    if (s_fullscreen) return;   // ignore in fullscreen mode
    if (w <= 0 || h <= 0) return;

    DWORD style   = (DWORD)GetWindowLongPtr(s_hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtr(s_hwnd, GWL_EXSTYLE);

    RECT rc = { 0, 0, (LONG)w, (LONG)h };
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
    int windowW = rc.right  - rc.left;
    int windowH = rc.bottom - rc.top;

    // Center on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - windowW) / 2;
    int posY = (screenH - windowH) / 2;

    SetWindowPos(s_hwnd, NULL, posX, posY, windowW, windowH,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    s_defaultW = w;
    s_defaultH = h;

    // Trigger viewport + mouse-bounds update
    SendMessage(s_hwnd, WM_EXITSIZEMOVE, 0, 0);
}
