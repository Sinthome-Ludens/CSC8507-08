/**
 * @file UI_ItemWheel.cpp
 * @brief 道具轮盘渲染实现（TAB 长按弹出，4 扇区选择，实时库存数据）。
 */
#include "UI_ItemWheel.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

using namespace ECS::UITheme;

static constexpr int kSectorCount = 4;

void RenderItemWheel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    if (!ui.itemWheelOpen) return;
    if (!registry.has_ctx<Res_GameState>()) return;
    auto& gs = registry.ctx<Res_GameState>();

    // Read slot data from GameState + live counts from inventory
    struct SectorData {
        const char* name;
        uint8_t count;
        bool isActive;
    };
    SectorData sectors[4] = {};

    const bool hasInv = registry.has_ctx<Res_ItemInventory2>();
    auto* inv = hasInv ? &registry.ctx<Res_ItemInventory2>() : nullptr;

    // Sector 0,1 = item slots; Sector 2,3 = weapon slots
    for (int s = 0; s < 2; ++s) {
        auto& slot = gs.itemSlots[s];
        sectors[s].name     = (slot.name[0] != '\0') ? slot.name : "---";
        sectors[s].count    = (inv && slot.name[0] != '\0' && slot.itemId < Res_ItemInventory2::kItemCount)
                              ? inv->slots[slot.itemId].carriedCount : slot.count;
        sectors[s].isActive = (gs.activeItemSlot == s && slot.name[0] != '\0');
    }
    for (int s = 0; s < 2; ++s) {
        auto& slot = gs.weaponSlots[s];
        sectors[2 + s].name     = (slot.name[0] != '\0') ? slot.name : "---";
        sectors[2 + s].count    = (inv && slot.name[0] != '\0' && slot.itemId < Res_ItemInventory2::kItemCount)
                                  ? inv->slots[slot.itemId].carriedCount : slot.count;
        sectors[2 + s].isActive = (gs.activeWeaponSlot == s && slot.name[0] != '\0');
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImFont* smallFont = GetFont_Small();

    float gameW = displaySize.x - Res_ChatState::kPanelWidth;
    float cx = gameW * 0.5f;
    float cy = displaySize.y * 0.5f;
    float outerR = 120.0f;
    float innerR = 40.0f;

    // Dim background
    draw->AddRectFilled(
        ImVec2(0.0f, 0.0f),
        ImVec2(displaySize.x, displaySize.y),
        Col32_BgDark(100));

    // Determine selected sector from mouse position
    ImVec2 mousePos = ImGui::GetMousePos();
    float dx = mousePos.x - cx;
    float dy = mousePos.y - cy;
    float dist = sqrtf(dx * dx + dy * dy);

    int hoveredSector = -1;
    if (dist > innerR * 0.5f) {
        float angle = atan2f(dy, dx);
        angle += kPI * 0.5f;
        if (angle < 0.0f) angle += kPI * 2.0f;
        hoveredSector = (int)(angle / (kPI * 2.0f) * kSectorCount) % kSectorCount;
        ui.itemWheelSelected = static_cast<int8_t>(hoveredSector);
    }

    // Draw sectors
    float sectorAngle = kPI * 2.0f / kSectorCount;
    for (int i = 0; i < kSectorCount; ++i) {
        float startAngle = i * sectorAngle - kPI * 0.5f;
        float endAngle   = startAngle + sectorAngle;
        bool isHovered = (i == hoveredSector);
        bool isActive  = sectors[i].isActive;

        ImU32 sectorColor = isHovered
            ? Col32_Accent(60)
            : isActive ? Col32_Accent(30) : Col32_Bg(180);

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

        // Active slot: thick orange arc border
        if (isActive) {
            for (int s = 0; s < kArcSegments; ++s) {
                float a1 = startAngle + s * da;
                float a2 = a1 + da;
                ImVec2 p1(cx + cosf(a1) * outerR, cy + sinf(a1) * outerR);
                ImVec2 p2(cx + cosf(a2) * outerR, cy + sinf(a2) * outerR);
                draw->AddLine(p1, p2, Col32_Accent(220), 3.0f);
            }
        }

        // Sector border lines
        ImVec2 lineInner(cx + cosf(startAngle) * innerR, cy + sinf(startAngle) * innerR);
        ImVec2 lineOuter(cx + cosf(startAngle) * outerR, cy + sinf(startAngle) * outerR);
        draw->AddLine(lineInner, lineOuter, Col32_Gray(100), 1.0f);

        // Label: name + count
        float midAngle = startAngle + sectorAngle * 0.5f;
        float labelR = (innerR + outerR) * 0.5f;
        float lx = cx + cosf(midAngle) * labelR;
        float ly = cy + sinf(midAngle) * labelR;

        if (smallFont) ImGui::PushFont(smallFont);

        // Name
        ImVec2 nameSize = ImGui::CalcTextSize(sectors[i].name);
        ImU32 nameCol = isHovered ? Col32_Accent(255)
                      : isActive  ? Col32_Accent(200)
                                  : Col32_Text(220);
        draw->AddText(ImVec2(lx - nameSize.x * 0.5f, ly - nameSize.y * 0.5f - 6.0f),
            nameCol, sectors[i].name);

        // Count (below name)
        if (sectors[i].count > 0) {
            char countBuf[8];
            snprintf(countBuf, sizeof(countBuf), "x%u", sectors[i].count);
            ImVec2 countSize = ImGui::CalcTextSize(countBuf);
            draw->AddText(ImVec2(lx - countSize.x * 0.5f, ly - countSize.y * 0.5f + 6.0f),
                Col32_Accent(180), countBuf);
        }

        if (smallFont) ImGui::PopFont();
    }

    // Center circle
    draw->AddCircleFilled(ImVec2(cx, cy), innerR * 0.8f,
        Col32_Bg(220), 32);
    draw->AddCircle(ImVec2(cx, cy), innerR * 0.8f,
        Col32_Gray(150), 32, 1.5f);

    // Center text
    if (smallFont) ImGui::PushFont(smallFont);
    const char* centerText = "SELECT";
    ImVec2 ctSize = ImGui::CalcTextSize(centerText);
    draw->AddText(ImVec2(cx - ctSize.x * 0.5f, cy - ctSize.y * 0.5f),
        Col32_Text(180), centerText);
    if (smallFont) ImGui::PopFont();

    // Outer ring
    draw->AddCircle(ImVec2(cx, cy), outerR,
        Col32_Gray(100), 64, 1.0f);
    draw->AddCircle(ImVec2(cx, cy), innerR,
        Col32_Gray(100), 32, 1.0f);

    // Category labels
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(cx - 20.0f, cy - outerR - 18.0f),
        Col32_Accent(160), "GADGETS");
    draw->AddText(ImVec2(cx - 28.0f, cy + outerR + 6.0f),
        Col32_Accent(160), "WEAPONS");
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
