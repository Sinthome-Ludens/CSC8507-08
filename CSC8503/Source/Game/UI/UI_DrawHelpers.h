/**
 * @file UI_DrawHelpers.h
 * @brief Shared drawing primitives for UI (panels, shadows, highlights, etc.)
 *
 * All color parameters use UITheme::Col32_* functions by default.
 * These helpers reduce repetition across UI_Menus, UI_HUD, UI_GameOver, etc.
 */
#pragma once
#ifdef USE_IMGUI

#include <imgui.h>

namespace ECS::UI::Draw {

/// Fill the entire screen with a solid color.
void FillBackground(ImDrawList* draw, ImU32 color);

/// Draw a rounded panel with optional border.
void Panel(ImDrawList* draw, ImVec2 min, ImVec2 max,
           ImU32 bgColor, ImU32 borderColor = 0,
           float rounding = 3.0f, float borderWidth = 1.0f);

/// Draw a horizontal separator line.
void Separator(ImDrawList* draw, float x0, float x1, float y,
               ImU32 color, float thickness = 1.0f);

/// Draw a highlighted rectangle behind a selected menu item.
void MenuItemHighlight(ImDrawList* draw, ImVec2 min, ImVec2 max,
                       ImU32 color, float rounding = 2.0f);

/// Draw a multi-layer offset rectangle simulating a drop shadow.
void DropShadow(ImDrawList* draw, ImVec2 min, ImVec2 max,
                float rounding = 3.0f, uint8_t baseAlpha = 40,
                int layers = 3, float offset = 3.0f);

/// Draw a glowing border (additive-style, multiple passes).
void GlowBorder(ImDrawList* draw, ImVec2 min, ImVec2 max,
                ImU32 color, float rounding = 3.0f,
                int passes = 3, float expand = 2.0f);

/// Draw bottom-centered hint text.
void BottomHint(ImDrawList* draw, const char* text, float displayW, float y,
                ImU32 color, ImFont* font = nullptr);

/// Draw a large page title with shadow.
void PageTitle(ImDrawList* draw, const char* text, ImVec2 pos,
               ImU32 textColor, ImU32 shadowColor, ImFont* font = nullptr);

} // namespace ECS::UI::Draw

#endif // USE_IMGUI
