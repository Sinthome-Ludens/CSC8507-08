/**
 * @file UITheme.h
 * @brief 集中化 UI 配色常量和样式参数（暖奶油 + 橙色强调）
 *
 * @note Called by Sys_UI::OnAwake() (ApplyTheme / LoadFonts)
 */
#pragma once
#ifdef USE_IMGUI

#include <imgui.h>

namespace ECS::UITheme {

// ============================================================
// Colors — Warm cream + orange accent (#F5EEE8 / #FC6F29)
// ============================================================

// Backgrounds (unified #F5EEE8 — fully opaque)
inline const ImVec4 kBgWindow         = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kBgChildWindow    = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kBgPopup          = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kBgMenuBar        = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kBgTitleBar       = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kBgTitleBarActive = {0.94f,  0.91f,  0.89f,  1.00f};

// Primary accent (#FC6F29 orange)
inline const ImVec4 kAccent           = {0.988f, 0.435f, 0.161f, 1.00f};
inline const ImVec4 kAccentHover      = {1.000f, 0.530f, 0.280f, 1.00f};
inline const ImVec4 kAccentPressed    = {0.900f, 0.380f, 0.120f, 1.00f};
inline const ImVec4 kAccentDim        = {0.988f, 0.435f, 0.161f, 0.50f};

// Text (#100D0A)
inline const ImVec4 kTextPrimary      = {0.063f, 0.051f, 0.039f, 1.00f};
inline const ImVec4 kTextSecondary    = {0.20f,  0.18f,  0.16f,  1.00f};
inline const ImVec4 kTextDisabled     = {0.55f,  0.55f,  0.55f,  1.00f};

// Borders
inline const ImVec4 kBorderNormal     = {0.78f,  0.76f,  0.74f,  0.80f};
inline const ImVec4 kBorderActive     = {0.988f, 0.435f, 0.161f, 0.80f};

// Buttons (#C8C8C8 normal / #FC6F29 active)
inline const ImVec4 kButton           = {0.784f, 0.784f, 0.784f, 1.00f};
inline const ImVec4 kButtonHover      = {0.72f,  0.72f,  0.72f,  1.00f};
inline const ImVec4 kButtonActive     = {0.988f, 0.435f, 0.161f, 1.00f};

// Sliders (orange accent)
inline const ImVec4 kSliderGrab       = {0.988f, 0.435f, 0.161f, 1.00f};
inline const ImVec4 kSliderGrabActive = {1.000f, 0.530f, 0.280f, 1.00f};

// Header
inline const ImVec4 kHeader           = {0.94f,  0.91f,  0.89f,  1.00f};
inline const ImVec4 kHeaderHover      = {0.90f,  0.87f,  0.85f,  1.00f};
inline const ImVec4 kHeaderActive     = {0.988f, 0.435f, 0.161f, 1.00f};

// Separator
inline const ImVec4 kSeparator        = {0.78f,  0.76f,  0.74f,  0.60f};

// Frame (input fields — #F5EEE8)
inline const ImVec4 kFrameBg          = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kFrameBgHover     = {0.94f,  0.91f,  0.89f,  1.00f};
inline const ImVec4 kFrameBgActive    = {0.92f,  0.89f,  0.87f,  1.00f};

// Tab
inline const ImVec4 kTab              = {0.94f,  0.91f,  0.89f,  1.00f};
inline const ImVec4 kTabHover         = {0.90f,  0.87f,  0.85f,  1.00f};
inline const ImVec4 kTabActive        = {0.988f, 0.435f, 0.161f, 1.00f};

// Scrollbar
inline const ImVec4 kScrollbarBg      = {0.961f, 0.933f, 0.910f, 1.00f};
inline const ImVec4 kScrollbarGrab    = {0.78f,  0.76f,  0.74f,  0.60f};

// ============================================================
// Math constants
// ============================================================

constexpr float kPI = 3.14159265f;

// ============================================================
// Style constants
// ============================================================

constexpr float kWindowRounding     = 2.0f;
constexpr float kFrameRounding      = 1.0f;
constexpr float kPopupRounding      = 2.0f;
constexpr float kScrollbarRounding  = 1.0f;
constexpr float kGrabRounding       = 1.0f;
constexpr float kTabRounding        = 1.0f;
constexpr float kWindowBorderSize   = 1.0f;
constexpr float kFrameBorderSize    = 0.0f;

inline const ImVec2 kWindowPadding     = {10.0f, 10.0f};
inline const ImVec2 kFramePadding      = {6.0f, 4.0f};
inline const ImVec2 kItemSpacing       = {8.0f, 6.0f};
inline const ImVec2 kItemInnerSpacing  = {6.0f, 4.0f};

// ============================================================
// Function declarations
// ============================================================

void ApplyTheme();
void LoadFonts();

ImFont* GetFont_Terminal();       // ZLabsRoundPix 16px
ImFont* GetFont_TerminalLarge();  // ZLabsRoundPix 32px
ImFont* GetFont_Body();           // ZLabsRoundPix 16px
ImFont* GetFont_Small();          // ZLabsRoundPix 13px

} // namespace ECS::UITheme

#endif // USE_IMGUI
