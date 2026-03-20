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
// ImU32 draw-list colors (for ImDrawList direct usage)
// ============================================================

/// @brief Warm cream background (#F5EEE8) as ImU32 with adjustable alpha.
inline ImU32 Col32_Bg(uint8_t a = 255)         { return IM_COL32(245,238,232,a); }
/// @brief Near-black warm dark (#100D0A) as ImU32 with adjustable alpha.
inline ImU32 Col32_BgDark(uint8_t a = 255)      { return IM_COL32(16,13,10,a); }
/// @brief Orange accent (#FC6F29) as ImU32 with adjustable alpha.
inline ImU32 Col32_Accent(uint8_t a = 255)      { return IM_COL32(252,111,41,a); }
/// @brief Lighter orange hover (#FF8747) as ImU32 with adjustable alpha.
inline ImU32 Col32_AccentHover(uint8_t a = 255) { return IM_COL32(255,135,71,a); }
/// @brief Primary text color (#100D0A) as ImU32 with adjustable alpha.
inline ImU32 Col32_Text(uint8_t a = 255)        { return IM_COL32(16,13,10,a); }
/// @brief Neutral gray (#C8C8C8) as ImU32 with adjustable alpha.
inline ImU32 Col32_Gray(uint8_t a = 255)        { return IM_COL32(200,200,200,a); }
/// @brief Status green (#50C878) as ImU32 with adjustable alpha.
inline ImU32 Col32_Green(uint8_t a = 220)       { return IM_COL32(80,200,120,a); }
/// @brief Status yellow (#DCC800) as ImU32 with adjustable alpha.
inline ImU32 Col32_Yellow(uint8_t a = 220)      { return IM_COL32(220,200,0,a); }
/// @brief Status red (#DC3C28) as ImU32 with adjustable alpha.
inline ImU32 Col32_Red(uint8_t a = 220)         { return IM_COL32(220,60,40,a); }

/// @brief 比率→绿/黄/红颜色（计时条/倒计时共用）。
inline ImU32 Col32_RatioColor(float ratio, uint8_t a = 220) {
    if      (ratio > 0.5f)  return Col32_Green(a);
    else if (ratio > 0.25f) return Col32_Yellow(a);
    else                    return Col32_Red(a);
}

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
// Layout constants
// ============================================================

namespace Layout {
    constexpr float kPagePadX       = 40.0f;
    constexpr float kPagePadTop     = 30.0f;
    constexpr float kMenuItemH      = 34.0f;
    constexpr float kPauseItemH     = 38.0f;
    constexpr float kSeparatorGap   = 20.0f;
    constexpr float kBottomHintY    = 35.0f;
    constexpr float kPanelRounding  = 3.0f;
    constexpr float kCardRounding   = 4.0f;
    constexpr float kBorderWidth    = 1.0f;
    constexpr float kSelectedBorderW = 2.0f;
    constexpr float kHUDPanelAlpha  = 180;

    namespace Chat {
        constexpr float kHeaderH        = 36.0f;
        constexpr float kHeaderPadX     = 12.0f;
        constexpr float kHeaderTextY    = 8.0f;
        constexpr float kModeTagPadX    = 6.0f;
        constexpr float kModeTagPadY    = 2.0f;
        constexpr float kModeTagY       = 10.0f;
        constexpr float kTimerBarH      = 4.0f;
        constexpr float kTimerBarGap    = 2.0f;
        constexpr float kMsgTopPad      = 6.0f;
        constexpr float kMsgPadX        = 10.0f;
        constexpr float kSenderGap      = 6.0f;
        constexpr float kReplyPad       = 8.0f;
        constexpr float kCountdownH     = 36.0f;
        constexpr float kInputBoxH      = 28.0f;
        constexpr float kReplySpacing   = 4.0f;
        constexpr float kReplyItemH     = 42.0f;
        constexpr float kHintLineH      = 24.0f;
        constexpr float kHintBottomY    = 18.0f;
        constexpr float kSlotSize       = 20.0f;
        constexpr float kSlotGap        = 4.0f;
        constexpr float kArrowCellW     = 18.0f;
        constexpr float kArrowX0        = 14.0f;
        constexpr float kArrowSize      = 5.0f;
        constexpr float kArrowYOffset   = 22.0f;
        constexpr float kArrowCenterY   = 7.0f;
        constexpr float kSepInset       = 8.0f;
        constexpr float kHighlightInset = 6.0f;
        constexpr float kHighlightH     = 40.0f;
        constexpr float kMsgLineH       = 22.0f;
    }
}

// ============================================================
// Score rating color (shared across HUD / GameOver / Victory)
// ============================================================

/// @brief 根据评级档位返回对应颜色（S+绿 / B+黄 / D+橙 / F红）。
inline ImU32 GetScoreRatingColor(int8_t tier, uint8_t alpha = 255) {
    if      (tier >= 5) return IM_COL32(80, 200, 120, alpha);   // S+ green
    else if (tier >= 3) return IM_COL32(220, 200, 0, alpha);    // B+ yellow
    else if (tier >= 1) return IM_COL32(252, 111, 41, alpha);   // D+ orange
    else                return IM_COL32(220, 60, 40, alpha);    // F  red
}

// ============================================================
// Function declarations
// ============================================================

void ApplyTheme();
void LoadFonts(float dpiScale = 1.0f);

ImFont* GetFont_Terminal();       // ZLabsRoundPix 16px * dpiScale
ImFont* GetFont_TerminalLarge();  // ZLabsRoundPix 32px * dpiScale
ImFont* GetFont_Body();           // ZLabsRoundPix 16px * dpiScale
ImFont* GetFont_Small();          // ZLabsRoundPix 13px * dpiScale

} // namespace ECS::UITheme

#endif // USE_IMGUI
