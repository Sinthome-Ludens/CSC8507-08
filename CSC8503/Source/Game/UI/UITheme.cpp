#include "UITheme.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Assets.h"
#include "Game/Utils/Log.h"

namespace ECS::UITheme {

// ============================================================
// Static font pointers
// ============================================================

static ImFont* s_FontTerminal      = nullptr;
static ImFont* s_FontTerminalLarge = nullptr;
static ImFont* s_FontBody          = nullptr;
static ImFont* s_FontSmall         = nullptr;

// ============================================================
// ApplyTheme
// ============================================================

void ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

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

    c[ImGuiCol_Text]                  = kTextPrimary;
    c[ImGuiCol_TextDisabled]          = kTextDisabled;

    c[ImGuiCol_WindowBg]              = kBgWindow;
    c[ImGuiCol_ChildBg]               = kBgChildWindow;
    c[ImGuiCol_PopupBg]               = kBgPopup;

    c[ImGuiCol_Border]                = kBorderNormal;
    c[ImGuiCol_BorderShadow]          = {0.0f, 0.0f, 0.0f, 0.0f};

    c[ImGuiCol_FrameBg]               = kFrameBg;
    c[ImGuiCol_FrameBgHovered]        = kFrameBgHover;
    c[ImGuiCol_FrameBgActive]         = kFrameBgActive;

    c[ImGuiCol_TitleBg]               = kBgTitleBar;
    c[ImGuiCol_TitleBgActive]         = kBgTitleBarActive;
    c[ImGuiCol_TitleBgCollapsed]      = kBgTitleBar;

    c[ImGuiCol_MenuBarBg]             = kBgMenuBar;

    c[ImGuiCol_ScrollbarBg]           = kScrollbarBg;
    c[ImGuiCol_ScrollbarGrab]         = kScrollbarGrab;
    c[ImGuiCol_ScrollbarGrabHovered]  = kAccentDim;
    c[ImGuiCol_ScrollbarGrabActive]   = kAccent;

    c[ImGuiCol_CheckMark]             = kAccent;

    c[ImGuiCol_SliderGrab]            = kSliderGrab;
    c[ImGuiCol_SliderGrabActive]      = kSliderGrabActive;

    c[ImGuiCol_Button]                = kButton;
    c[ImGuiCol_ButtonHovered]         = kButtonHover;
    c[ImGuiCol_ButtonActive]          = kButtonActive;

    c[ImGuiCol_Header]                = kHeader;
    c[ImGuiCol_HeaderHovered]         = kHeaderHover;
    c[ImGuiCol_HeaderActive]          = kHeaderActive;

    c[ImGuiCol_Separator]             = kSeparator;
    c[ImGuiCol_SeparatorHovered]      = kAccentDim;
    c[ImGuiCol_SeparatorActive]       = kAccent;

    c[ImGuiCol_ResizeGrip]            = kBorderNormal;
    c[ImGuiCol_ResizeGripHovered]     = kAccentDim;
    c[ImGuiCol_ResizeGripActive]      = kAccent;

    c[ImGuiCol_Tab]                   = kTab;
    c[ImGuiCol_TabHovered]            = kTabHover;
    c[ImGuiCol_TabSelected]           = kTabActive;

    c[ImGuiCol_PlotLines]             = kAccentDim;
    c[ImGuiCol_PlotLinesHovered]      = kAccent;
    c[ImGuiCol_PlotHistogram]         = kAccentDim;
    c[ImGuiCol_PlotHistogramHovered]  = kAccent;

    c[ImGuiCol_TextSelectedBg]        = {0.00f, 0.00f, 0.00f, 0.20f};

    c[ImGuiCol_DragDropTarget]        = kAccentHover;

    c[ImGuiCol_NavHighlight]          = kAccent;
    c[ImGuiCol_NavWindowingHighlight] = {1.0f, 1.0f, 1.0f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg]     = {0.80f, 0.80f, 0.80f, 0.20f};

    c[ImGuiCol_ModalWindowDimBg]      = {0.0f, 0.0f, 0.0f, 0.70f};

    LOG_INFO("[UITheme] Clean white + black accent theme applied.");
}

// ============================================================
// LoadFonts
// ============================================================

void LoadFonts() {
    if (s_FontTerminal) return;

    ImGuiIO& io = ImGui::GetIO();

    const std::string cousinePath = NCL::Assets::FONTSSDIR + "Cousine-Regular.ttf";
    const std::string robotoPath  = NCL::Assets::FONTSSDIR + "Roboto-Medium.ttf";

    s_FontTerminal = io.Fonts->AddFontFromFileTTF(cousinePath.c_str(), 16.0f);
    if (!s_FontTerminal) {
        LOG_WARN("[UITheme] Failed to load Cousine-Regular 16px, using default.");
        s_FontTerminal = io.Fonts->AddFontDefault();
    }

    s_FontTerminalLarge = io.Fonts->AddFontFromFileTTF(cousinePath.c_str(), 28.0f);
    if (!s_FontTerminalLarge) {
        LOG_WARN("[UITheme] Failed to load Cousine-Regular 28px, using default.");
        s_FontTerminalLarge = io.Fonts->AddFontDefault();
    }

    s_FontBody = io.Fonts->AddFontFromFileTTF(robotoPath.c_str(), 16.0f);
    if (!s_FontBody) {
        LOG_WARN("[UITheme] Failed to load Roboto-Medium 16px, using default.");
        s_FontBody = io.Fonts->AddFontDefault();
    }

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
