#include "UITheme.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Assets.h"
#include "Game/Utils/Log.h"

namespace ECS::UITheme {

// ============================================================
// 静态字体指针
// ============================================================

static ImFont* s_FontTerminal      = nullptr;
static ImFont* s_FontTerminalLarge = nullptr;
static ImFont* s_FontBody          = nullptr;
static ImFont* s_FontSmall         = nullptr;

// ============================================================
// ApplyCyberpunkTheme
// ============================================================

void ApplyCyberpunkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // 样式
    style.WindowRounding    = kWindowRounding;
    style.FrameRounding     = kFrameRounding;
    style.PopupRounding     = kPopupRounding;
    style.ScrollbarRounding = kScrollbarRounding;
    style.GrabRounding      = kGrabRounding;
    style.TabRounding       = kTabRounding;
    style.WindowBorderSize  = kWindowBorderSize;
    style.FrameBorderSize   = kFrameBorderSize;
    style.WindowPadding     = kWindowPadding;
    style.FramePadding      = kFramePadding;
    style.ItemSpacing       = kItemSpacing;
    style.ItemInnerSpacing  = kItemInnerSpacing;

    ImVec4* c = style.Colors;

    // 文字
    c[ImGuiCol_Text]                  = kTextPrimary;
    c[ImGuiCol_TextDisabled]          = kTextDisabled;

    // 窗口
    c[ImGuiCol_WindowBg]              = kBgWindow;
    c[ImGuiCol_ChildBg]               = kBgChildWindow;
    c[ImGuiCol_PopupBg]               = kBgPopup;

    // 边框
    c[ImGuiCol_Border]                = kBorderNormal;
    c[ImGuiCol_BorderShadow]          = {0.0f, 0.0f, 0.0f, 0.0f};

    // 输入框
    c[ImGuiCol_FrameBg]               = kFrameBg;
    c[ImGuiCol_FrameBgHovered]        = kFrameBgHover;
    c[ImGuiCol_FrameBgActive]         = kFrameBgActive;

    // 标题栏
    c[ImGuiCol_TitleBg]               = kBgTitleBar;
    c[ImGuiCol_TitleBgActive]         = kBgTitleBarActive;
    c[ImGuiCol_TitleBgCollapsed]      = kBgTitleBar;

    // 菜单栏
    c[ImGuiCol_MenuBarBg]             = kBgMenuBar;

    // 滚动条
    c[ImGuiCol_ScrollbarBg]           = kScrollbarBg;
    c[ImGuiCol_ScrollbarGrab]         = kScrollbarGrab;
    c[ImGuiCol_ScrollbarGrabHovered]  = kCyanDim;
    c[ImGuiCol_ScrollbarGrabActive]   = kCyan;

    // 复选/单选
    c[ImGuiCol_CheckMark]             = kCyan;

    // 滑块
    c[ImGuiCol_SliderGrab]            = kSliderGrab;
    c[ImGuiCol_SliderGrabActive]      = kSliderGrabActive;

    // 按钮
    c[ImGuiCol_Button]                = kButton;
    c[ImGuiCol_ButtonHovered]         = kButtonHover;
    c[ImGuiCol_ButtonActive]          = kButtonActive;

    // Header
    c[ImGuiCol_Header]                = kHeader;
    c[ImGuiCol_HeaderHovered]         = kHeaderHover;
    c[ImGuiCol_HeaderActive]          = kHeaderActive;

    // 分隔线
    c[ImGuiCol_Separator]             = kSeparator;
    c[ImGuiCol_SeparatorHovered]      = kCyanDim;
    c[ImGuiCol_SeparatorActive]       = kCyan;

    // 缩放手柄
    c[ImGuiCol_ResizeGrip]            = kBorderNormal;
    c[ImGuiCol_ResizeGripHovered]     = kCyanDim;
    c[ImGuiCol_ResizeGripActive]      = kCyan;

    // Tab
    c[ImGuiCol_Tab]                   = kTab;
    c[ImGuiCol_TabHovered]            = kTabHover;
    c[ImGuiCol_TabSelected]           = kTabActive;

    // 绘图
    c[ImGuiCol_PlotLines]             = kCyanDim;
    c[ImGuiCol_PlotLinesHovered]      = kCyan;
    c[ImGuiCol_PlotHistogram]         = kMilitaryGreen;
    c[ImGuiCol_PlotHistogramHovered]  = kMilitaryGreenHover;

    // 文本选中
    c[ImGuiCol_TextSelectedBg]        = {0.00f, 0.50f, 0.47f, 0.35f};

    // 拖放目标
    c[ImGuiCol_DragDropTarget]        = kCyanHover;

    // 导航
    c[ImGuiCol_NavHighlight]          = kCyan;
    c[ImGuiCol_NavWindowingHighlight] = {1.0f, 1.0f, 1.0f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg]     = {0.80f, 0.80f, 0.80f, 0.20f};

    // 模态暗化
    c[ImGuiCol_ModalWindowDimBg]      = {0.0f, 0.0f, 0.0f, 0.70f};

    LOG_INFO("[UITheme] Cyberpunk theme applied.");
}

// ============================================================
// LoadFonts
// ============================================================

void LoadFonts() {
    // 防止场景切换时重复加载（OnAwake 每次场景进入都会调用）
    if (s_FontTerminal) return;

    ImGuiIO& io = ImGui::GetIO();

    const std::string cousinePath = NCL::Assets::FONTSSDIR + "Cousine-Regular.ttf";
    const std::string robotoPath  = NCL::Assets::FONTSSDIR + "Roboto-Medium.ttf";

    // Terminal — Cousine-Regular 16px（终端主字体）
    s_FontTerminal = io.Fonts->AddFontFromFileTTF(cousinePath.c_str(), 16.0f);
    if (!s_FontTerminal) {
        LOG_WARN("[UITheme] Failed to load Cousine-Regular 16px, using default.");
        s_FontTerminal = io.Fonts->AddFontDefault();
    }

    // TerminalLarge — Cousine-Regular 28px（标题）
    s_FontTerminalLarge = io.Fonts->AddFontFromFileTTF(cousinePath.c_str(), 28.0f);
    if (!s_FontTerminalLarge) {
        LOG_WARN("[UITheme] Failed to load Cousine-Regular 28px, using default.");
        s_FontTerminalLarge = io.Fonts->AddFontDefault();
    }

    // Body — Roboto-Medium 16px（UI正文）
    s_FontBody = io.Fonts->AddFontFromFileTTF(robotoPath.c_str(), 16.0f);
    if (!s_FontBody) {
        LOG_WARN("[UITheme] Failed to load Roboto-Medium 16px, using default.");
        s_FontBody = io.Fonts->AddFontDefault();
    }

    // Small — Roboto-Medium 13px（标签/说明）
    s_FontSmall = io.Fonts->AddFontFromFileTTF(robotoPath.c_str(), 13.0f);
    if (!s_FontSmall) {
        LOG_WARN("[UITheme] Failed to load Roboto-Medium 13px, using default.");
        s_FontSmall = io.Fonts->AddFontDefault();
    }

    io.Fonts->Build();

    LOG_INFO("[UITheme] Fonts loaded: Terminal(16), TerminalLarge(28), Body(16), Small(13).");
}

// ============================================================
// Font Getters
// ============================================================

ImFont* GetFont_Terminal()      { return s_FontTerminal; }
ImFont* GetFont_TerminalLarge() { return s_FontTerminalLarge; }
ImFont* GetFont_Body()          { return s_FontBody; }
ImFont* GetFont_Small()         { return s_FontSmall; }

} // namespace ECS::UITheme

#endif // USE_IMGUI
