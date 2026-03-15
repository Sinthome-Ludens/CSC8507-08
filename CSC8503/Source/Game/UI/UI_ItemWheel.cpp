/**
 * @file UI_ItemWheel.cpp
 * @brief 道具轮盘渲染实现（TAB 长按弹出，4 扇区选择）。
 */
#include "UI_ItemWheel.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

static constexpr int kSectorCount = 4;

/**
 * @brief 渲染 4 扇区径向道具轮盘（TAB 长按弹出，居中于游戏区域）。
 * @param registry ECS 注册表
 * @param dt       帧间隔（未使用）
 */
void RenderItemWheel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    if (!ui.itemWheelOpen) return;

    // Read slot names from Res_GameState
    const char* sectorNames[4] = { "---", "---", "---", "---" };
    if (registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();
        // Sector 0,1 = items; Sector 2,3 = weapons
        sectorNames[0] = (gs.itemSlots[0].name[0] != '\0') ? gs.itemSlots[0].name : "---";
        sectorNames[1] = (gs.itemSlots[1].name[0] != '\0') ? gs.itemSlots[1].name : "---";
        sectorNames[2] = (gs.weaponSlots[0].name[0] != '\0') ? gs.weaponSlots[0].name : "---";
        sectorNames[3] = (gs.weaponSlots[1].name[0] != '\0') ? gs.weaponSlots[1].name : "---";
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImFont* smallFont = UITheme::GetFont_Small();

    // Center in game area (excluding chat panel)
    float gameW = displaySize.x - Res_ChatState::kPanelWidth;
    float cx = gameW * 0.5f;
    float cy = displaySize.y * 0.5f;
    float outerR = 120.0f;
    float innerR = 40.0f;

    // Dim background
    draw->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(displaySize.x, displaySize.y),
        IM_COL32(16, 13, 10, 100));

    // Determine selected sector from mouse position
    ImVec2 mousePos = ImGui::GetMousePos();
    float dx = mousePos.x - cx;
    float dy = mousePos.y - cy;
    float dist = sqrtf(dx * dx + dy * dy);

    int hoveredSector = -1;
    if (dist > innerR * 0.5f) {
        float angle = atan2f(dy, dx);
        angle += UITheme::kPI * 0.5f;
        if (angle < 0.0f) angle += UITheme::kPI * 2.0f;
        hoveredSector = (int)(angle / (UITheme::kPI * 2.0f) * kSectorCount) % kSectorCount;
        ui.itemWheelSelected = static_cast<int8_t>(hoveredSector);
    }

    // Draw sectors
    float sectorAngle = UITheme::kPI * 2.0f / kSectorCount;
    for (int i = 0; i < kSectorCount; ++i) {
        float startAngle = i * sectorAngle - UITheme::kPI * 0.5f;
        float endAngle   = startAngle + sectorAngle;
        bool isHovered = (i == hoveredSector);

        ImU32 sectorColor = isHovered
            ? IM_COL32(252, 111, 41, 60)
            : IM_COL32(245, 238, 232, 180);
        ImU32 borderColor = isHovered
            ? IM_COL32(252, 111, 41, 200)
            : IM_COL32(200, 200, 200, 120);

        constexpr int kArcSegments = 12;
        float da = (endAngle - startAngle) / kArcSegments;
        for (int s = 0; s < kArcSegments; ++s) {
            float a1 = startAngle + s * da;
            float a2 = a1 + da;
            ImVec2 p1(cx + cosf(a1) * innerR, cy + sinf(a1) * innerR);
            ImVec2 p2(cx + cosf(a1) * outerR, cy + sinf(a1) * outerR);
            ImVec2 p3(cx + cosf(a2) * outerR, cy + sinf(a2) * outerR);
            ImVec2 p4(cx + cosf(a2) * innerR, cy + sinf(a2) * innerR);
            draw->AddQuadFilled(p1, p2, p3, p4, sectorColor);
        }

        // Sector border lines
        ImVec2 lineInner(cx + cosf(startAngle) * innerR, cy + sinf(startAngle) * innerR);
        ImVec2 lineOuter(cx + cosf(startAngle) * outerR, cy + sinf(startAngle) * outerR);
        draw->AddLine(lineInner, lineOuter, IM_COL32(200, 200, 200, 100), 1.0f);

        // Label (read from game state)
        float midAngle = startAngle + sectorAngle * 0.5f;
        float labelR = (innerR + outerR) * 0.5f;
        float lx = cx + cosf(midAngle) * labelR;
        float ly = cy + sinf(midAngle) * labelR;

        if (smallFont) ImGui::PushFont(smallFont);
        ImVec2 textSize = ImGui::CalcTextSize(sectorNames[i]);
        draw->AddText(ImVec2(lx - textSize.x * 0.5f, ly - textSize.y * 0.5f),
            isHovered ? IM_COL32(252, 111, 41, 255) : IM_COL32(16, 13, 10, 220),
            sectorNames[i]);
        if (smallFont) ImGui::PopFont();
    }

    // Center circle
    draw->AddCircleFilled(ImVec2(cx, cy), innerR * 0.8f,
        IM_COL32(245, 238, 232, 220), 32);
    draw->AddCircle(ImVec2(cx, cy), innerR * 0.8f,
        IM_COL32(200, 200, 200, 150), 32, 1.5f);

    // Center text
    if (smallFont) ImGui::PushFont(smallFont);
    const char* centerText = "SELECT";
    ImVec2 ctSize = ImGui::CalcTextSize(centerText);
    draw->AddText(ImVec2(cx - ctSize.x * 0.5f, cy - ctSize.y * 0.5f),
        IM_COL32(16, 13, 10, 180), centerText);
    if (smallFont) ImGui::PopFont();

    // Outer ring
    draw->AddCircle(ImVec2(cx, cy), outerR,
        IM_COL32(200, 200, 200, 100), 64, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), innerR,
        IM_COL32(200, 200, 200, 100), 32, 1.0f);

    // Category labels
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(cx - 20.0f, cy - outerR - 18.0f),
        IM_COL32(252, 111, 41, 160), "ITEMS");
    draw->AddText(ImVec2(cx - 28.0f, cy + outerR + 6.0f),
        IM_COL32(252, 111, 41, 160), "WEAPONS");
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
