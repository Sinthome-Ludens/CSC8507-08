#include "UITheme.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <fstream>
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

    c[ImGuiCol_TextSelectedBg]        = {0.063f, 0.051f, 0.039f, 0.20f};

    c[ImGuiCol_DragDropTarget]        = kAccentHover;

    c[ImGuiCol_NavHighlight]          = kAccent;
    c[ImGuiCol_NavWindowingHighlight] = {1.0f, 1.0f, 1.0f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg]     = {0.80f, 0.80f, 0.80f, 0.20f};

    c[ImGuiCol_ModalWindowDimBg]      = {0.0f, 0.0f, 0.0f, 0.70f};

    LOG_INFO("[UITheme] Warm cream + orange accent theme applied.");
}

// ============================================================
// LoadFonts
// ============================================================

// 检查文件是否存在（AddFontFromFileTTF 在部分 ImGui 版本中文件缺失也返回非空指针）
static bool FileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// 安全加载字体：先检查文件存在性，再调用 AddFontFromFileTTF
static ImFont* SafeLoadFont(ImGuiIO& io, const std::string& path, float size,
                            const char* name, const ImWchar* glyphRanges = nullptr) {
    if (!FileExists(path)) {
        LOG_WARN("[UITheme] Font file not found: " << path << ", using default for " << name);
        return io.Fonts->AddFontDefault();
    }
    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH  = true;
    cfg.GlyphRanges = glyphRanges;
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size, &cfg, glyphRanges);
    if (!font) {
        LOG_WARN("[UITheme] Failed to load " << name << ", using default.");
        return io.Fonts->AddFontDefault();
    }
    return font;
}

/// @brief Load all UI fonts at the given DPI scale (call once during init).
///
/// Fonts are cached in static pointers; subsequent calls are no-ops.
/// A dpiScale < 0.5 is clamped to 1.0 as a safety fallback.
void LoadFonts(float dpiScale) {
    if (s_FontTerminal) return;
    if (dpiScale < 0.5f) dpiScale = 1.0f;

    ImGuiIO& io = ImGui::GetIO();

    const std::string pixelFontPath = NCL::Assets::FONTSSDIR + "ZLabsRoundPix_16px_M_CN.ttf";
    const ImWchar* cnRanges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();

    s_FontTerminal      = SafeLoadFont(io, pixelFontPath, 16.0f * dpiScale, "ZLabsRoundPix 16px", cnRanges);
    s_FontTerminalLarge = SafeLoadFont(io, pixelFontPath, 32.0f * dpiScale, "ZLabsRoundPix 32px", cnRanges);
    s_FontBody          = SafeLoadFont(io, pixelFontPath, 16.0f * dpiScale, "ZLabsRoundPix 16px", cnRanges);
    s_FontSmall         = SafeLoadFont(io, pixelFontPath, 13.0f * dpiScale, "ZLabsRoundPix 13px", cnRanges);

    io.Fonts->Build();

    LOG_INFO("[UITheme] Fonts loaded (dpiScale=" << dpiScale << "): ZLabsRoundPix — Terminal(16), Large(32), Body(16), Small(13).");
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
