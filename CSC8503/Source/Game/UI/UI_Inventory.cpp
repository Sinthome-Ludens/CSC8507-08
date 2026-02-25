#include "UI_Inventory.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

void RenderInventoryScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_InventoryState>()) return;

    auto& ui  = registry.ctx<Res_UIState>();
    auto& inv = registry.ctx<Res_InventoryState>();

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

    // 半透明暗化背景
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(4, 6, 10, 210));

    // 面板居中
    float panelW = 520.0f;
    float panelH = 440.0f;
    float panelX = vpPos.x + (vpSize.x - panelW) * 0.5f;
    float panelY = vpPos.y + (vpSize.y - panelH) * 0.5f;

    // 面板背景
    draw->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(8, 12, 18, 245), 3.0f);
    draw->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        IM_COL32(0, 140, 130, 100), 3.0f, 0, 1.0f);

    // 标题
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(panelX + 25.0f, panelY + 20.0f),
        IM_COL32(0, 220, 210, 255), "INVENTORY");
    if (titleFont) ImGui::PopFont();

    // 分隔线
    float lineY = panelY + 60.0f;
    draw->AddLine(
        ImVec2(panelX + 20.0f, lineY),
        ImVec2(panelX + panelW - 20.0f, lineY),
        IM_COL32(0, 140, 130, 80), 1.0f);

    // ── 网格布局（4列 x 3行 = 12槽）──
    float gridStartX = panelX + 20.0f;
    float gridStartY = lineY + 15.0f;
    float cardW = 115.0f;
    float cardH = 90.0f;
    float gapX  = 8.0f;
    float gapY  = 8.0f;
    int cols = 4;

    // 键盘导航
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.inventorySelectedSlot = static_cast<int8_t>(
                (ui.inventorySelectedSlot - cols + Res_InventoryState::MAX_SLOTS)
                % Res_InventoryState::MAX_SLOTS);
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.inventorySelectedSlot = static_cast<int8_t>(
                (ui.inventorySelectedSlot + cols) % Res_InventoryState::MAX_SLOTS);
        }
        if (kb->KeyPressed(KeyCodes::A) || kb->KeyPressed(KeyCodes::LEFT)) {
            ui.inventorySelectedSlot = static_cast<int8_t>(
                (ui.inventorySelectedSlot - 1 + Res_InventoryState::MAX_SLOTS)
                % Res_InventoryState::MAX_SLOTS);
        }
        if (kb->KeyPressed(KeyCodes::D) || kb->KeyPressed(KeyCodes::RIGHT)) {
            ui.inventorySelectedSlot = static_cast<int8_t>(
                (ui.inventorySelectedSlot + 1) % Res_InventoryState::MAX_SLOTS);
        }
    }

    const Mouse* mouse = Window::GetMouse();

    ImFont* termFont = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    for (int i = 0; i < Res_InventoryState::MAX_SLOTS; ++i) {
        int col = i % cols;
        int row = i / cols;
        float cx = gridStartX + col * (cardW + gapX);
        float cy = gridStartY + row * (cardH + gapY);

        bool isSelected = (i == ui.inventorySelectedSlot);
        const auto& slot = inv.slots[i];
        bool isEmpty = (slot.type == 0);

        // 鼠标检测
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= cx && mousePos.x <= cx + cardW &&
                mousePos.y >= cy && mousePos.y <= cy + cardH) {
                ui.inventorySelectedSlot = static_cast<int8_t>(i);
            }
        }

        // 卡片背景
        ImU32 cardBg = isEmpty ? IM_COL32(12, 16, 22, 200)
                               : IM_COL32(15, 20, 28, 230);
        ImU32 cardBorder = isSelected ? IM_COL32(0, 220, 210, 200)
                                      : IM_COL32(0, 80, 75, 80);
        draw->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
            cardBg, 3.0f);
        draw->AddRect(ImVec2(cx, cy), ImVec2(cx + cardW, cy + cardH),
            cardBorder, 3.0f, 0, 1.0f);

        if (isEmpty) {
            // 空槽位
            if (smallFont) ImGui::PushFont(smallFont);
            draw->AddText(ImVec2(cx + 8.0f, cy + cardH * 0.5f - 6.0f),
                IM_COL32(40, 45, 50, 150), "EMPTY");
            if (smallFont) ImGui::PopFont();
        } else {
            // 类型标签
            if (smallFont) ImGui::PushFont(smallFont);
            const char* typeLabel = (slot.type == 1) ? "ITEM" : "WEAPON";
            ImU32 typeColor = (slot.type == 1) ? IM_COL32(0, 180, 170, 150)
                                               : IM_COL32(220, 140, 0, 150);
            draw->AddText(ImVec2(cx + 6.0f, cy + 4.0f), typeColor, typeLabel);
            if (smallFont) ImGui::PopFont();

            // 物品名
            if (termFont) ImGui::PushFont(termFont);
            ImU32 nameColor = isSelected ? IM_COL32(0, 220, 210, 255)
                                         : IM_COL32(180, 190, 200, 220);
            draw->AddText(ImVec2(cx + 6.0f, cy + 20.0f), nameColor, slot.name);
            if (termFont) ImGui::PopFont();

            // 数量
            if (smallFont) ImGui::PushFont(smallFont);
            char countBuf[16];
            if (slot.count < 0) {
                snprintf(countBuf, sizeof(countBuf), "INF");
            } else {
                snprintf(countBuf, sizeof(countBuf), "x%d", slot.count);
            }
            draw->AddText(ImVec2(cx + 6.0f, cy + 40.0f),
                IM_COL32(100, 110, 120, 180), countBuf);

            // 描述
            draw->AddText(ImVec2(cx + 6.0f, cy + 58.0f),
                IM_COL32(80, 90, 100, 150), slot.desc);
            if (smallFont) ImGui::PopFont();
        }
    }

    // ── 详情面板（选中物品信息）──
    float detailY = gridStartY + 3 * (cardH + gapY) + 10.0f;
    if (ui.inventorySelectedSlot >= 0 &&
        ui.inventorySelectedSlot < Res_InventoryState::MAX_SLOTS) {
        const auto& sel = inv.slots[ui.inventorySelectedSlot];
        if (sel.type != 0) {
            if (termFont) ImGui::PushFont(termFont);
            char detailBuf[64];
            snprintf(detailBuf, sizeof(detailBuf), ">> %s — %s", sel.name, sel.desc);
            draw->AddText(ImVec2(panelX + 25.0f, detailY),
                IM_COL32(0, 200, 190, 200), detailBuf);
            if (termFont) ImGui::PopFont();
        }
    }

    // 底部提示
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(panelX + 20.0f, panelY + panelH - 25.0f),
        IM_COL32(60, 70, 75, 150),
        "[WASD] Navigate  [I/ESC] Close");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
