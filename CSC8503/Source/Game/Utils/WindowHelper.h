/**
 * @file WindowHelper.h
 * @brief Game-layer platform adapter for window management (fullscreen, resize)
 *
 * @details
 * Bridges ECS layer (Res_UIState) and platform window API, isolating Win32
 * implementation details from game systems. Avoids modifying the shared
 * NCLCoreClasses engine layer.
 *
 * ## Design
 *
 * - `Init()` performs a one-time `dynamic_cast<Win32Window*>` and caches the
 *   result. All subsequent calls are O(1) pointer dereferences.
 * - Viewport / render-target / mouse-bounds updates are triggered automatically
 *   by sending `WM_EXITSIZEMOVE` after `SetWindowPos`, which activates the
 *   `applyResize` path in `Win32Window::WindowProc`.
 * - If `Init()` fails (non-Win32 platform), all methods become safe no-ops.
 *
 * @see Sys_UI (reads Res_UIState and calls WindowHelper)
 * @see Main.cpp (owns the init/call-site)
 */
#pragma once

namespace NCL { class Window; }

class WindowHelper {
public:
    /// Call once at startup. Returns false if dynamic_cast fails.
    static bool Init(NCL::Window* window);

    /// Borderless fullscreen toggle. Viewport auto-updates via WM_EXITSIZEMOVE.
    static void SetFullScreen(bool fullscreen);

    /// Resize windowed mode (no-op when fullscreen). Centers window on screen.
    static void SetWindowSize(int w, int h);
};
