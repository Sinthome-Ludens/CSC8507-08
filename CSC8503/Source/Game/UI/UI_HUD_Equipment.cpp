/**
 * @file UI_HUD_Equipment.cpp
 * @brief HUD sub-module: bottom-center dual equipment panels (Q gadget + E weapon).
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_ItemIcons.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

void ItemSlots(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH) {
    ImFont* smallFont = GetFont_Small();
    ImFont* termFont  = GetFont_Terminal();

    constexpr float kPanelW = 140.0f;
    constexpr float kPanelH = 60.0f;
    constexpr float kGap    = 10.0f;
    constexpr float kIconSize = 12.0f;

    float totalW = 2 * kPanelW + kGap;
    float startX = gameW * 0.5f - totalW * 0.5f;
    float panelY = displayH - kPanelH - 24.0f;

    struct PanelData {
        const SlotDisplay* slot;
        const char* keyLabel;
        const char* typeLabel;
        bool isFlashing;
    };

    const uint8_t safeItemSlot   = (gs.activeItemSlot   < 2) ? gs.activeItemSlot   : 0;
    const uint8_t safeWeaponSlot = (gs.activeWeaponSlot < 2) ? gs.activeWeaponSlot : 0;

    PanelData panels[2] = {
        { &gs.itemSlots[safeItemSlot],     "[Q]", "GADGET",
          gs.itemUseFlashTimer > 0.0f && gs.itemUseFlashSlotType == 0 },
        { &gs.weaponSlots[safeWeaponSlot], "[E]", "WEAPON",
          gs.itemUseFlashTimer > 0.0f && gs.itemUseFlashSlotType == 1 },
    };

    for (int i = 0; i < 2; ++i) {
        float px = startX + i * (kPanelW + kGap);
        ImVec2 panelMin(px, panelY);
        ImVec2 panelMax(px + kPanelW, panelY + kPanelH);

        const auto* slot = panels[i].slot;
        bool hasItem = (slot->name[0] != '\0');
        bool onCooldown = (slot->cooldown > 0.01f);

        // Background
        draw->AddRectFilled(panelMin, panelMax, Col32_BgDark(140), Layout::kPanelRounding);

        // Border (flash = bright orange, normal = orange, empty = gray)
        if (panels[i].isFlashing) {
            uint8_t flashAlpha = (uint8_t)(200 + 55 * gs.itemUseFlashTimer / 0.3f);
            draw->AddRect(panelMin, panelMax, Col32_Accent(flashAlpha), Layout::kPanelRounding, 0, 3.0f);
        } else if (hasItem) {
            draw->AddRect(panelMin, panelMax, Col32_Accent(200), Layout::kPanelRounding, 0, 1.5f);
        } else {
            draw->AddRect(panelMin, panelMax, Col32_Gray(60), Layout::kPanelRounding, 0, 1.0f);
        }

        if (hasItem) {
            // Icon (left side)
            ImVec2 iconCenter(px + 20.0f, panelY + 20.0f);
            if (slot->itemId < static_cast<uint8_t>(ItemID::Count)) {
                DrawItemIcon(draw, iconCenter, kIconSize, static_cast<ItemID>(slot->itemId),
                    Col32_Bg(220));
            }

            // Name
            if (termFont) ImGui::PushFont(termFont);
            draw->AddText(ImVec2(px + 38.0f, panelY + 4.0f),
                Col32_Bg(220), slot->name);
            if (termFont) ImGui::PopFont();

            // Count + status
            if (smallFont) ImGui::PushFont(smallFont);
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), "x%u", slot->count);
            draw->AddText(ImVec2(px + 38.0f, panelY + 20.0f),
                Col32_Accent(200), countBuf);

            // READY / COOLDOWN label (game mechanic colors)
            ImVec2 countSize = ImGui::CalcTextSize(countBuf);
            if (onCooldown) {
                draw->AddText(ImVec2(px + 38.0f + countSize.x + 8.0f, panelY + 20.0f),
                    IM_COL32(220, 60, 40, 200), "WAIT");
            } else if (slot->count > 0) {
                draw->AddText(ImVec2(px + 38.0f + countSize.x + 8.0f, panelY + 20.0f),
                    IM_COL32(80, 200, 120, 200), "READY");
            }
            if (smallFont) ImGui::PopFont();

            // Cooldown progress bar (game mechanic colors)
            if (onCooldown) {
                float cdFill = std::clamp(slot->cooldown, 0.0f, 1.0f);
                float barW = kPanelW * cdFill;
                draw->AddRectFilled(
                    ImVec2(px, panelY + kPanelH - 4.0f),
                    ImVec2(px + barW, panelY + kPanelH),
                    IM_COL32(220, 60, 40, 180), 0.0f);
            } else {
                draw->AddRectFilled(
                    ImVec2(px, panelY + kPanelH - 4.0f),
                    ImVec2(px + kPanelW, panelY + kPanelH),
                    IM_COL32(80, 200, 120, 100), 0.0f);
            }
        } else {
            // Empty slot
            if (smallFont) ImGui::PushFont(smallFont);
            draw->AddText(ImVec2(px + kPanelW * 0.5f - 10.0f, panelY + kPanelH * 0.5f - 6.0f),
                Col32_Gray(100), "---");
            if (smallFont) ImGui::PopFont();
        }

        // Key label + type label
        if (smallFont) ImGui::PushFont(smallFont);
        char labelBuf[24];
        snprintf(labelBuf, sizeof(labelBuf), "%s %s", panels[i].keyLabel, panels[i].typeLabel);
        ImVec2 labelSize = ImGui::CalcTextSize(labelBuf);
        draw->AddText(ImVec2(px + kPanelW * 0.5f - labelSize.x * 0.5f, panelY + kPanelH - 16.0f),
            Col32_Bg(120), labelBuf);
        if (smallFont) ImGui::PopFont();
    }
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
