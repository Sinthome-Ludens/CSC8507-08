#include "UI_Inventory.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// RenderInventoryScreen — 4x3 grid item cards + detail panel
// ============================================================

void RenderInventoryScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##Inventory", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    // Title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(vpPos.x + 40.0f, vpPos.y + 30.0f),
        IM_COL32(16, 13, 10, 255), "INVENTORY");
    if (titleFont) ImGui::PopFont();

    float headerLineY = vpPos.y + 70.0f;
    draw->AddLine(
        ImVec2(vpPos.x + 40.0f, headerLineY),
        ImVec2(vpPos.x + vpSize.x - 40.0f, headerLineY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // Res_InventoryState 由 Sys_UI::OnAwake 初始化
    if (!registry.has_ctx<Res_InventoryState>()) return;
    auto& inv = registry.ctx<Res_InventoryState>();

    // Grid layout: 4 columns x 3 rows
    constexpr float kCardW = 120.0f;
    constexpr float kCardH = 100.0f;
    constexpr float kGapX  = 12.0f;
    constexpr float kGapY  = 12.0f;

    float gridTotalW = Res_InventoryState::kCols * kCardW + (Res_InventoryState::kCols - 1) * kGapX;
    float gridStartX = vpPos.x + 40.0f;
    float gridStartY = headerLineY + 20.0f;

    // Detail panel on the right
    float detailX = gridStartX + gridTotalW + 30.0f;
    float detailW = vpSize.x - (detailX - vpPos.x) - 40.0f;

    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    // Keyboard navigation
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        int sel = ui.inventorySelectedSlot;
        int row = sel / Res_InventoryState::kCols;
        int col = sel % Res_InventoryState::kCols;

        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            row = (row - 1 + Res_InventoryState::kRows) % Res_InventoryState::kRows;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            row = (row + 1) % Res_InventoryState::kRows;
        }
        if (kb->KeyPressed(KeyCodes::A) || kb->KeyPressed(KeyCodes::LEFT)) {
            col = (col - 1 + Res_InventoryState::kCols) % Res_InventoryState::kCols;
        }
        if (kb->KeyPressed(KeyCodes::D) || kb->KeyPressed(KeyCodes::RIGHT)) {
            col = (col + 1) % Res_InventoryState::kCols;
        }
        ui.inventorySelectedSlot = static_cast<int8_t>(row * Res_InventoryState::kCols + col);
    }

    // Mouse hover
    const Mouse* mouse = Window::GetMouse();

    // Draw grid cards
    for (int r = 0; r < Res_InventoryState::kRows; ++r) {
        for (int c = 0; c < Res_InventoryState::kCols; ++c) {
            int idx = r * Res_InventoryState::kCols + c;
            float cardX = gridStartX + c * (kCardW + kGapX);
            float cardY = gridStartY + r * (kCardH + kGapY);

            ImVec2 cardMin(cardX, cardY);
            ImVec2 cardMax(cardX + kCardW, cardY + kCardH);

            bool isSelected = (idx == ui.inventorySelectedSlot);

            // Mouse hover detection
            if (mouse) {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.x >= cardMin.x && mousePos.x <= cardMax.x &&
                    mousePos.y >= cardMin.y && mousePos.y <= cardMax.y) {
                    ui.inventorySelectedSlot = static_cast<int8_t>(idx);
                    isSelected = true;
                }
            }

            // Card background
            draw->AddRectFilled(cardMin, cardMax,
                IM_COL32(245, 238, 232, 255), 3.0f);

            if (isSelected) {
                draw->AddRect(cardMin, cardMax,
                    IM_COL32(252, 111, 41, 180), 3.0f, 0, 2.0f);
            } else {
                draw->AddRect(cardMin, cardMax,
                    IM_COL32(200, 200, 200, 120), 3.0f, 0, 1.0f);
            }

            // Slot content
            const auto& item = inv.slots[idx];
            if (!item.isEmpty) {
                if (termFont) ImGui::PushFont(termFont);
                draw->AddText(ImVec2(cardX + 8.0f, cardY + 8.0f),
                    IM_COL32(16, 13, 10, 220), item.name);
                if (termFont) ImGui::PopFont();

                if (item.quantity > 1) {
                    if (smallFont) ImGui::PushFont(smallFont);
                    char qtyBuf[8];
                    snprintf(qtyBuf, sizeof(qtyBuf), "x%u", item.quantity);
                    draw->AddText(ImVec2(cardX + 8.0f, cardY + kCardH - 22.0f),
                        IM_COL32(252, 111, 41, 200), qtyBuf);
                    if (smallFont) ImGui::PopFont();
                }
            } else {
                // Empty slot
                if (smallFont) ImGui::PushFont(smallFont);
                draw->AddText(ImVec2(cardX + 8.0f, cardY + kCardH * 0.5f - 6.0f),
                    IM_COL32(200, 200, 200, 120), "EMPTY");
                if (smallFont) ImGui::PopFont();
            }

            // Slot index number
            if (smallFont) ImGui::PushFont(smallFont);
            char idxBuf[4];
            snprintf(idxBuf, sizeof(idxBuf), "%d", idx + 1);
            draw->AddText(ImVec2(cardX + kCardW - 18.0f, cardY + 4.0f),
                IM_COL32(16, 13, 10, 80), idxBuf);
            if (smallFont) ImGui::PopFont();
        }
    }

    // Detail panel
    {
        float dpY = gridStartY;
        draw->AddRectFilled(
            ImVec2(detailX, dpY),
            ImVec2(detailX + detailW, dpY + kCardH * 3 + kGapY * 2),
            IM_COL32(245, 238, 232, 255), 3.0f);
        draw->AddRect(
            ImVec2(detailX, dpY),
            ImVec2(detailX + detailW, dpY + kCardH * 3 + kGapY * 2),
            IM_COL32(200, 200, 200, 100), 3.0f);

        if (termFont) ImGui::PushFont(termFont);
        draw->AddText(ImVec2(detailX + 12.0f, dpY + 12.0f),
            IM_COL32(252, 111, 41, 220), "DETAILS");
        if (termFont) ImGui::PopFont();

        float detailLineY = dpY + 38.0f;
        draw->AddLine(
            ImVec2(detailX + 12.0f, detailLineY),
            ImVec2(detailX + detailW - 12.0f, detailLineY),
            IM_COL32(200, 200, 200, 80), 1.0f);

        int selIdx = ui.inventorySelectedSlot;
        if (selIdx >= 0 && selIdx < Res_InventoryState::kSlotCount) {
            const auto& selItem = inv.slots[selIdx];
            if (!selItem.isEmpty) {
                if (termFont) ImGui::PushFont(termFont);
                draw->AddText(ImVec2(detailX + 12.0f, detailLineY + 12.0f),
                    IM_COL32(16, 13, 10, 255), selItem.name);
                if (termFont) ImGui::PopFont();

                if (smallFont) ImGui::PushFont(smallFont);
                draw->AddText(ImVec2(detailX + 12.0f, detailLineY + 36.0f),
                    IM_COL32(16, 13, 10, 180), selItem.description);

                char qtyLine[32];
                snprintf(qtyLine, sizeof(qtyLine), "QUANTITY: %u", selItem.quantity);
                draw->AddText(ImVec2(detailX + 12.0f, detailLineY + 60.0f),
                    IM_COL32(16, 13, 10, 160), qtyLine);
                if (smallFont) ImGui::PopFont();
            } else {
                if (smallFont) ImGui::PushFont(smallFont);
                draw->AddText(ImVec2(detailX + 12.0f, detailLineY + 12.0f),
                    IM_COL32(200, 200, 200, 160), "NO ITEM SELECTED");
                if (smallFont) ImGui::PopFont();
            }
        }
    }

    // Bottom hint
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(vpPos.x + 40.0f, vpPos.y + vpSize.y - 30.0f),
        IM_COL32(16, 13, 10, 180),
        "[W/A/S/D] NAVIGATE  [I] CLOSE  [ESC] BACK");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
