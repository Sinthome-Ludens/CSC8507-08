#pragma once
#ifdef USE_IMGUI

#include <imgui.h>

namespace ECS::UITheme {

// ============================================================
// 颜色常量 — 赛博朋克终端 + MGS战术风格
// ============================================================

// 背景系列（近黑 + 微蓝色调）
constexpr ImVec4 kBgWindow       = {0.04f, 0.05f, 0.08f, 0.98f};
constexpr ImVec4 kBgChildWindow  = {0.05f, 0.06f, 0.09f, 0.95f};
constexpr ImVec4 kBgPopup        = {0.06f, 0.07f, 0.10f, 0.97f};
constexpr ImVec4 kBgMenuBar      = {0.03f, 0.04f, 0.06f, 0.98f};
constexpr ImVec4 kBgTitleBar     = {0.05f, 0.06f, 0.09f, 0.98f};
constexpr ImVec4 kBgTitleBarActive = {0.07f, 0.10f, 0.14f, 1.00f};

// 主色调（青色/蓝绿色 — 终端发光色）
constexpr ImVec4 kCyan           = {0.00f, 0.85f, 0.80f, 1.00f};
constexpr ImVec4 kCyanHover      = {0.00f, 1.00f, 0.95f, 1.00f};
constexpr ImVec4 kCyanPressed    = {0.00f, 0.65f, 0.60f, 1.00f};
constexpr ImVec4 kCyanDim        = {0.00f, 0.50f, 0.47f, 1.00f};

// 辅助色调（橄榄绿/军事绿 — MGS风格）
constexpr ImVec4 kMilitaryGreen      = {0.30f, 0.45f, 0.20f, 1.00f};
constexpr ImVec4 kMilitaryGreenHover = {0.40f, 0.55f, 0.25f, 1.00f};

// 文字系列
constexpr ImVec4 kTextPrimary    = {0.85f, 0.90f, 0.92f, 1.00f};
constexpr ImVec4 kTextSecondary  = {0.50f, 0.55f, 0.58f, 1.00f};
constexpr ImVec4 kTextDisabled   = {0.30f, 0.33f, 0.35f, 1.00f};
constexpr ImVec4 kTextWarning    = {0.90f, 0.70f, 0.20f, 1.00f};  // 琥珀色
constexpr ImVec4 kTextDanger     = {0.90f, 0.25f, 0.20f, 1.00f};  // 红色

// 边框系列
constexpr ImVec4 kBorderNormal   = {0.00f, 0.30f, 0.28f, 0.60f};
constexpr ImVec4 kBorderActive   = {0.00f, 0.85f, 0.80f, 0.80f};

// 按钮
constexpr ImVec4 kButton         = {0.08f, 0.12f, 0.16f, 1.00f};
constexpr ImVec4 kButtonHover    = {0.10f, 0.18f, 0.24f, 1.00f};
constexpr ImVec4 kButtonActive   = {0.00f, 0.60f, 0.55f, 1.00f};

// 滑块/进度条
constexpr ImVec4 kSliderGrab     = {0.00f, 0.70f, 0.65f, 1.00f};
constexpr ImVec4 kSliderGrabActive = {0.00f, 0.90f, 0.85f, 1.00f};

// Header
constexpr ImVec4 kHeader         = {0.08f, 0.15f, 0.18f, 1.00f};
constexpr ImVec4 kHeaderHover    = {0.10f, 0.20f, 0.24f, 1.00f};
constexpr ImVec4 kHeaderActive   = {0.00f, 0.50f, 0.47f, 1.00f};

// 分隔线
constexpr ImVec4 kSeparator      = {0.00f, 0.25f, 0.23f, 0.50f};

// 输入框
constexpr ImVec4 kFrameBg        = {0.06f, 0.08f, 0.11f, 1.00f};
constexpr ImVec4 kFrameBgHover   = {0.08f, 0.12f, 0.16f, 1.00f};
constexpr ImVec4 kFrameBgActive  = {0.10f, 0.15f, 0.20f, 1.00f};

// Tab
constexpr ImVec4 kTab            = {0.06f, 0.08f, 0.11f, 1.00f};
constexpr ImVec4 kTabHover       = {0.10f, 0.18f, 0.24f, 1.00f};
constexpr ImVec4 kTabActive      = {0.00f, 0.50f, 0.47f, 1.00f};

// Scrollbar
constexpr ImVec4 kScrollbarBg    = {0.03f, 0.04f, 0.06f, 0.80f};
constexpr ImVec4 kScrollbarGrab  = {0.00f, 0.30f, 0.28f, 0.60f};

// ============================================================
// 样式常量
// ============================================================

constexpr float kWindowRounding   = 2.0f;
constexpr float kFrameRounding    = 1.0f;
constexpr float kPopupRounding    = 2.0f;
constexpr float kScrollbarRounding = 1.0f;
constexpr float kGrabRounding     = 1.0f;
constexpr float kTabRounding      = 1.0f;
constexpr float kWindowBorderSize = 1.0f;
constexpr float kFrameBorderSize  = 0.0f;

constexpr ImVec2 kWindowPadding   = {10.0f, 10.0f};
constexpr ImVec2 kFramePadding    = {6.0f, 4.0f};
constexpr ImVec2 kItemSpacing     = {8.0f, 6.0f};
constexpr ImVec2 kItemInnerSpacing = {6.0f, 4.0f};

// ============================================================
// 函数声明
// ============================================================

void ApplyCyberpunkTheme();
void LoadFonts();

ImFont* GetFont_Terminal();       // Cousine-Regular 16px
ImFont* GetFont_TerminalLarge();  // Cousine-Regular 28px
ImFont* GetFont_Body();           // Roboto-Medium 16px
ImFont* GetFont_Small();          // Roboto-Medium 13px

} // namespace ECS::UITheme

#endif // USE_IMGUI
