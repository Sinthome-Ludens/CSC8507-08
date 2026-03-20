/**
 * @file UI_DrawHelpers.cpp
 * @brief Shared drawing primitives implementation.
 */
#include "UI_DrawHelpers.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/UI/UITheme.h"

namespace ECS::UI::Draw {

/// @brief Fill the entire display area with a solid color.
void FillBackground(ImDrawList* draw, ImU32 color) {
    const ImVec2 sz = ImGui::GetIO().DisplaySize;
    draw->AddRectFilled(ImVec2(0, 0), sz, color);
}

/// @brief Draw a rounded rectangle panel with optional border.
///
/// Border is skipped when borderColor alpha is zero.
void Panel(ImDrawList* draw, ImVec2 min, ImVec2 max,
           ImU32 bgColor, ImU32 borderColor,
           float rounding, float borderWidth) {
    draw->AddRectFilled(min, max, bgColor, rounding);
    if ((borderColor & IM_COL32_A_MASK) != 0) {
        draw->AddRect(min, max, borderColor, rounding, 0, borderWidth);
    }
}

/// @brief Draw a horizontal separator line from x0 to x1 at height y.
void Separator(ImDrawList* draw, float x0, float x1, float y,
               ImU32 color, float thickness) {
    draw->AddLine(ImVec2(x0, y), ImVec2(x1, y), color, thickness);
}

/// @brief Draw a filled rounded rect as a menu-item highlight backdrop.
void MenuItemHighlight(ImDrawList* draw, ImVec2 min, ImVec2 max,
                       ImU32 color, float rounding) {
    draw->AddRectFilled(min, max, color, rounding);
}

/// @brief Draw a multi-layer drop shadow behind a rectangle.
///
/// Each layer is offset by (offset * i) and alpha is divided by layer index.
void DropShadow(ImDrawList* draw, ImVec2 min, ImVec2 max,
                float rounding, uint8_t baseAlpha,
                int layers, float offset) {
    for (int i = layers; i >= 1; --i) {
        float o = offset * (float)i;
        uint8_t a = (uint8_t)(baseAlpha / i);
        draw->AddRectFilled(
            ImVec2(min.x + o, min.y + o),
            ImVec2(max.x + o, max.y + o),
            IM_COL32(0, 0, 0, a), rounding);
    }
}

void GlowBorder(ImDrawList* draw, ImVec2 min, ImVec2 max,
                ImU32 color, float rounding,
                int passes, float expand) {
    for (int i = passes; i >= 1; --i) {
        float e = expand * (float)i;
        // Extract alpha and reduce per pass
        uint8_t a = (uint8_t)(((color >> IM_COL32_A_SHIFT) & 0xFF) / (i + 1));
        ImU32 c = (color & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
        draw->AddRect(
            ImVec2(min.x - e, min.y - e),
            ImVec2(max.x + e, max.y + e),
            c, rounding + e * 0.5f, 0, 1.0f + e * 0.3f);
    }
    // Inner border at full alpha
    draw->AddRect(min, max, color, rounding, 0, 1.0f);
}

void BottomHint(ImDrawList* draw, const char* text, float displayW, float y,
                ImU32 color, ImFont* font) {
    if (font) ImGui::PushFont(font);
    ImVec2 sz = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(displayW * 0.5f - sz.x * 0.5f, y), color, text);
    if (font) ImGui::PopFont();
}

void PageTitle(ImDrawList* draw, const char* text, ImVec2 pos,
               ImU32 textColor, ImU32 shadowColor, ImFont* font) {
    if (font) ImGui::PushFont(font);
    // Shadow offset
    draw->AddText(ImVec2(pos.x + 2.0f, pos.y + 2.0f), shadowColor, text);
    draw->AddText(pos, textColor, text);
    if (font) ImGui::PopFont();
}

} // namespace ECS::UI::Draw

#endif // USE_IMGUI
