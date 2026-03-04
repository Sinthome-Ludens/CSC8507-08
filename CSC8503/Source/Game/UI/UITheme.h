#pragma once
#ifdef USE_IMGUI

#include <imgui.h>

namespace ECS::UITheme {

// ============================================================
// Colors — Clean white + black accent
// ============================================================

// Backgrounds (white / near-white)
constexpr ImVec4 kBgWindow         = {0.97f, 0.97f, 0.98f, 0.98f};
constexpr ImVec4 kBgChildWindow    = {0.96f, 0.96f, 0.97f, 0.95f};
constexpr ImVec4 kBgPopup          = {0.98f, 0.98f, 0.99f, 0.97f};
constexpr ImVec4 kBgMenuBar        = {0.95f, 0.95f, 0.96f, 0.98f};
constexpr ImVec4 kBgTitleBar       = {0.94f, 0.94f, 0.96f, 0.98f};
constexpr ImVec4 kBgTitleBarActive = {0.92f, 0.93f, 0.95f, 1.00f};

// Primary accent (black)
constexpr ImVec4 kAccent           = {0.00f, 0.00f, 0.00f, 1.00f};
constexpr ImVec4 kAccentHover      = {0.20f, 0.20f, 0.20f, 1.00f};
constexpr ImVec4 kAccentPressed    = {0.10f, 0.10f, 0.10f, 1.00f};
constexpr ImVec4 kAccentDim        = {0.35f, 0.35f, 0.35f, 1.00f};

// Text (black)
constexpr ImVec4 kTextPrimary      = {0.00f, 0.00f, 0.00f, 1.00f};
constexpr ImVec4 kTextSecondary    = {0.25f, 0.25f, 0.25f, 1.00f};
constexpr ImVec4 kTextDisabled     = {0.55f, 0.55f, 0.55f, 1.00f};

// Borders (light gray)
constexpr ImVec4 kBorderNormal     = {0.84f, 0.86f, 0.89f, 0.80f};
constexpr ImVec4 kBorderActive     = {0.00f, 0.00f, 0.00f, 0.80f};

// Buttons (light bg + black accent)
constexpr ImVec4 kButton           = {0.93f, 0.94f, 0.95f, 1.00f};
constexpr ImVec4 kButtonHover      = {0.86f, 0.87f, 0.88f, 1.00f};
constexpr ImVec4 kButtonActive     = {0.10f, 0.10f, 0.10f, 1.00f};

// Sliders (black)
constexpr ImVec4 kSliderGrab       = {0.00f, 0.00f, 0.00f, 1.00f};
constexpr ImVec4 kSliderGrabActive = {0.20f, 0.20f, 0.20f, 1.00f};

// Header (light gray)
constexpr ImVec4 kHeader           = {0.90f, 0.91f, 0.93f, 1.00f};
constexpr ImVec4 kHeaderHover      = {0.86f, 0.88f, 0.90f, 1.00f};
constexpr ImVec4 kHeaderActive     = {0.10f, 0.10f, 0.10f, 1.00f};

// Separator
constexpr ImVec4 kSeparator        = {0.84f, 0.86f, 0.89f, 0.60f};

// Frame (input fields)
constexpr ImVec4 kFrameBg          = {0.94f, 0.95f, 0.96f, 1.00f};
constexpr ImVec4 kFrameBgHover     = {0.91f, 0.92f, 0.94f, 1.00f};
constexpr ImVec4 kFrameBgActive    = {0.88f, 0.89f, 0.91f, 1.00f};

// Tab
constexpr ImVec4 kTab              = {0.94f, 0.95f, 0.96f, 1.00f};
constexpr ImVec4 kTabHover         = {0.89f, 0.90f, 0.92f, 1.00f};
constexpr ImVec4 kTabActive        = {0.10f, 0.10f, 0.10f, 1.00f};

// Scrollbar
constexpr ImVec4 kScrollbarBg      = {0.96f, 0.96f, 0.97f, 0.80f};
constexpr ImVec4 kScrollbarGrab    = {0.84f, 0.86f, 0.89f, 0.60f};

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

constexpr ImVec2 kWindowPadding     = {10.0f, 10.0f};
constexpr ImVec2 kFramePadding      = {6.0f, 4.0f};
constexpr ImVec2 kItemSpacing       = {8.0f, 6.0f};
constexpr ImVec2 kItemInnerSpacing  = {6.0f, 4.0f};

// ============================================================
// Function declarations
// ============================================================

void ApplyTheme();
void LoadFonts();

ImFont* GetFont_Terminal();       // Cousine-Regular 16px
ImFont* GetFont_TerminalLarge();  // Cousine-Regular 28px
ImFont* GetFont_Body();           // Roboto-Medium 16px
ImFont* GetFont_Small();          // Roboto-Medium 13px

} // namespace ECS::UITheme

#endif // USE_IMGUI
