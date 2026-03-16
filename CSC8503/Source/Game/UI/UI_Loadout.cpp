/**
 * @file UI_Loadout.cpp
 * @brief 武器/装备选择 UI。
 */
#include "UI_Loadout.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Toast.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Input.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// RenderLoadoutScreen — Equipment selection (dual-column)
// ============================================================

struct LoadoutEntry {
    const char* name;
    const char* description;
};

static const LoadoutEntry kItems[] = {
    {"LOCKPICK",     "Bypass electronic locks silently."},
    {"EMP GRENADE",  "Disable electronics in radius."},
    {"SMOKE BOMB",   "Create visual cover for 8 sec."},
    {"MED KIT",      "Restore 50% health."},
};
static constexpr int kItemCount = 4;

static const LoadoutEntry kWeapons[] = {
    {"SILENCED PISTOL",  "Low noise, moderate damage."},
    {"STUN GUN",         "Non-lethal. Single target."},
    {"CROSSBOW",         "Silent ranged. Limited ammo."},
    {"COMBAT KNIFE",     "Melee. Instant takedown."},
};
static constexpr int kWeaponCount = 4;

static constexpr int kTotalEntries = kItemCount + kWeaponCount;

// Equipped state lives in Res_UIState (loadoutEquippedItems/Weapons/Initialized)

void RenderLoadoutScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    // Reset equipped state when first entering loadout
    if (!ui.loadoutInitialized) {
        ui.loadoutEquippedItems[0] = ui.loadoutEquippedItems[1] = -1;
        ui.loadoutEquippedWeapons[0] = ui.loadoutEquippedWeapons[1] = -1;
        ui.loadoutInitialized = true;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##Loadout", nullptr,
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
        IM_COL32(16, 13, 10, 255), "LOADOUT");
    if (titleFont) ImGui::PopFont();

    float headerLineY = vpPos.y + 70.0f;
    draw->AddLine(
        ImVec2(vpPos.x + 40.0f, headerLineY),
        ImVec2(vpPos.x + vpSize.x - 40.0f, headerLineY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // Keyboard navigation
    const auto& input = registry.ctx<Res_Input>();
    {
        if (input.keyPressed[KeyCodes::W] || input.keyPressed[KeyCodes::UP]) {
            ui.loadoutSelectedIndex = (ui.loadoutSelectedIndex - 1 + kTotalEntries) % kTotalEntries;
        }
        if (input.keyPressed[KeyCodes::S] || input.keyPressed[KeyCodes::DOWN]) {
            ui.loadoutSelectedIndex = (ui.loadoutSelectedIndex + 1) % kTotalEntries;
        }
    }

    // Layout — two columns
    float colW = (vpSize.x - 120.0f) * 0.5f;
    float leftX = vpPos.x + 40.0f;
    float rightX = leftX + colW + 40.0f;
    float startY = headerLineY + 20.0f;

    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    // Column headers
    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(leftX, startY), IM_COL32(252, 111, 41, 220), "ITEMS (MAX 2)");
    draw->AddText(ImVec2(rightX, startY), IM_COL32(252, 111, 41, 220), "WEAPONS (MAX 2)");
    if (termFont) ImGui::PopFont();

    float entryStartY = startY + 30.0f;
    float entryH = 50.0f;

    // Helper: check if index is equipped
    auto isItemEquipped = [&](int idx) -> bool {
        return ui.loadoutEquippedItems[0] == idx || ui.loadoutEquippedItems[1] == idx;
    };
    auto isWeaponEquipped = [&](int idx) -> bool {
        return ui.loadoutEquippedWeapons[0] == idx || ui.loadoutEquippedWeapons[1] == idx;
    };
    auto countEquippedItems = [&]() -> int {
        int c = 0;
        if (ui.loadoutEquippedItems[0] >= 0) c++;
        if (ui.loadoutEquippedItems[1] >= 0) c++;
        return c;
    };
    auto countEquippedWeapons = [&]() -> int {
        int c = 0;
        if (ui.loadoutEquippedWeapons[0] >= 0) c++;
        if (ui.loadoutEquippedWeapons[1] >= 0) c++;
        return c;
    };

    // Mouse hover + click
    int8_t confirmedIndex = -1;

    auto drawColumn = [&](const LoadoutEntry* entries, int count, int indexOffset,
                          float colX, float colWidth, bool isWeaponCol) {
        for (int i = 0; i < count; ++i) {
            int globalIdx = indexOffset + i;
            float itemY = entryStartY + i * entryH;
            bool isSelected = (globalIdx == ui.loadoutSelectedIndex);
            bool equipped = isWeaponCol ? isWeaponEquipped(i) : isItemEquipped(i);

            ImVec2 itemMin(colX - 5.0f, itemY - 4.0f);
            ImVec2 itemMax(colX + colWidth, itemY + entryH - 8.0f);

            // Mouse hover
            {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                    mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                    ui.loadoutSelectedIndex = static_cast<int8_t>(globalIdx);
                    isSelected = true;
                }
            }

            // Background highlight
            if (equipped) {
                draw->AddRectFilled(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 40), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 180), 2.0f, 0, 1.5f);
            } else if (isSelected) {
                draw->AddRectFilled(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 25), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);
            }

            // Name
            if (termFont) ImGui::PushFont(termFont);
            char nameBuf[64];
            if (equipped) {
                snprintf(nameBuf, sizeof(nameBuf), "> %s [EQUIPPED]", entries[i].name);
            } else {
                snprintf(nameBuf, sizeof(nameBuf), isSelected ? "> %s" : "  %s", entries[i].name);
            }
            ImU32 nameColor = equipped ? IM_COL32(252, 111, 41, 255)
                            : isSelected ? IM_COL32(16, 13, 10, 255)
                            : IM_COL32(16, 13, 10, 220);
            draw->AddText(ImVec2(colX + (isSelected || equipped ? 4.0f : 0.0f), itemY),
                nameColor, nameBuf);
            if (termFont) ImGui::PopFont();

            // Description
            if (smallFont) ImGui::PushFont(smallFont);
            draw->AddText(ImVec2(colX + 18.0f, itemY + 20.0f),
                IM_COL32(16, 13, 10, 150), entries[i].description);
            if (smallFont) ImGui::PopFont();
        }
    };

    drawColumn(kItems, kItemCount, 0, leftX, colW, false);
    drawColumn(kWeapons, kWeaponCount, kItemCount, rightX, colW, true);

    // Confirm with ENTER/SPACE — toggle equip
    if (input.keyPressed[KeyCodes::RETURN] || input.keyPressed[KeyCodes::SPACE]) {
        confirmedIndex = ui.loadoutSelectedIndex;
    }
    if (input.mouseButtonPressed[NCL::MouseButtons::Left]) {
        ImVec2 mp = ImGui::GetMousePos();
        auto hitTest = [&](int count, int indexOffset, float colX, float colWidth) -> bool {
            for (int i = 0; i < count; ++i) {
                float itemY = entryStartY + i * entryH;
                ImVec2 itemMin(colX - 5.0f, itemY - 4.0f);
                ImVec2 itemMax(colX + colWidth, itemY + entryH - 8.0f);
                if (mp.x >= itemMin.x && mp.x <= itemMax.x &&
                    mp.y >= itemMin.y && mp.y <= itemMax.y) {
                    confirmedIndex = static_cast<int8_t>(indexOffset + i);
                    return true;
                }
            }
            return false;
        };
        if (!hitTest(kItemCount, 0, leftX, colW))
            hitTest(kWeaponCount, kItemCount, rightX, colW);
    }

    // Process equip toggle
    if (confirmedIndex >= 0) {
        if (confirmedIndex < kItemCount) {
            // Item toggle
            int idx = confirmedIndex;
            if (isItemEquipped(idx)) {
                // Unequip
                if (ui.loadoutEquippedItems[0] == idx) ui.loadoutEquippedItems[0] = -1;
                else if (ui.loadoutEquippedItems[1] == idx) ui.loadoutEquippedItems[1] = -1;
                LOG_INFO("[UI_Loadout] Unequipped item: " << kItems[idx].name);
            } else if (countEquippedItems() < 2) {
                // Equip
                if (ui.loadoutEquippedItems[0] < 0) ui.loadoutEquippedItems[0] = (int8_t)idx;
                else ui.loadoutEquippedItems[1] = (int8_t)idx;
                LOG_INFO("[UI_Loadout] Equipped item: " << kItems[idx].name);
            } else {
                PushToast(registry, "MAX 2 ITEMS — UNEQUIP FIRST", ToastType::Warning);
            }
        } else {
            // Weapon toggle
            int idx = confirmedIndex - kItemCount;
            if (isWeaponEquipped(idx)) {
                // Unequip
                if (ui.loadoutEquippedWeapons[0] == idx) ui.loadoutEquippedWeapons[0] = -1;
                else if (ui.loadoutEquippedWeapons[1] == idx) ui.loadoutEquippedWeapons[1] = -1;
                LOG_INFO("[UI_Loadout] Unequipped weapon: " << kWeapons[idx].name);
            } else if (countEquippedWeapons() < 2) {
                // Equip
                if (ui.loadoutEquippedWeapons[0] < 0) ui.loadoutEquippedWeapons[0] = (int8_t)idx;
                else ui.loadoutEquippedWeapons[1] = (int8_t)idx;
                LOG_INFO("[UI_Loadout] Equipped weapon: " << kWeapons[idx].name);
            } else {
                PushToast(registry, "MAX 2 WEAPONS — UNEQUIP FIRST", ToastType::Warning);
            }
        }
    }

    // Divider between columns
    float divX = leftX + colW + 15.0f;
    draw->AddLine(
        ImVec2(divX, startY),
        ImVec2(divX, entryStartY + kItemCount * entryH),
        IM_COL32(200, 200, 200, 80), 1.0f);

    // ── CONFIRM LOADOUT button ────────────────────────────
    float btnW = 240.0f;
    float btnH = 36.0f;
    float btnX = vpPos.x + (vpSize.x - btnW) * 0.5f;
    float btnY = vpPos.y + vpSize.y - 80.0f;
    ImVec2 btnMin(btnX, btnY);
    ImVec2 btnMax(btnX + btnW, btnY + btnH);

    // Check hover
    bool btnHovered = false;
    {
        ImVec2 mp = ImGui::GetMousePos();
        btnHovered = (mp.x >= btnMin.x && mp.x <= btnMax.x &&
                      mp.y >= btnMin.y && mp.y <= btnMax.y);
    }

    draw->AddRectFilled(btnMin, btnMax,
        btnHovered ? IM_COL32(252, 111, 41, 200) : IM_COL32(252, 111, 41, 160), 3.0f);
    draw->AddRect(btnMin, btnMax,
        IM_COL32(252, 111, 41, 255), 3.0f);

    if (termFont) ImGui::PushFont(termFont);
    const char* btnText = "CONFIRM LOADOUT";
    ImVec2 btnTextSize = ImGui::CalcTextSize(btnText);
    draw->AddText(
        ImVec2(btnX + (btnW - btnTextSize.x) * 0.5f, btnY + (btnH - btnTextSize.y) * 0.5f),
        IM_COL32(245, 238, 232, 255), btnText);
    if (termFont) ImGui::PopFont();

    // Confirm button click (mouse or C key)
    bool btnClicked = false;
    if (input.mouseButtonPressed[NCL::MouseButtons::Left] && btnHovered) {
        btnClicked = true;
    }
    if (input.keyPressed[KeyCodes::C]) {
        btnClicked = true;
    }

    if (btnClicked && registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();

        // Write items
        for (int s = 0; s < 2; ++s) {
            if (ui.loadoutEquippedItems[s] >= 0) {
                const char* src = kItems[ui.loadoutEquippedItems[s]].name;
                size_t len = strlen(src);
                if (len > sizeof(gs.itemSlots[s].name) - 1)
                    len = sizeof(gs.itemSlots[s].name) - 1;
                memcpy(gs.itemSlots[s].name, src, len);
                gs.itemSlots[s].name[len] = '\0';
                gs.itemSlots[s].count = 1;
            } else {
                gs.itemSlots[s].name[0] = '\0';
                gs.itemSlots[s].count = 0;
            }
            gs.itemSlots[s].cooldown = 0.0f;
        }

        // Write weapons
        for (int s = 0; s < 2; ++s) {
            if (ui.loadoutEquippedWeapons[s] >= 0) {
                const char* src = kWeapons[ui.loadoutEquippedWeapons[s]].name;
                size_t len = strlen(src);
                if (len > sizeof(gs.weaponSlots[s].name) - 1)
                    len = sizeof(gs.weaponSlots[s].name) - 1;
                memcpy(gs.weaponSlots[s].name, src, len);
                gs.weaponSlots[s].name[len] = '\0';
                gs.weaponSlots[s].count = 1;
            } else {
                gs.weaponSlots[s].name[0] = '\0';
                gs.weaponSlots[s].count = 0;
            }
            gs.weaponSlots[s].cooldown = 0.0f;
        }

        PushToast(registry, "LOADOUT CONFIRMED", ToastType::Success);
        LOG_INFO("[UI_Loadout] Loadout confirmed and written to GameState");

        // Reset for next visit
        ui.loadoutInitialized = false;
    }

    // Bottom hint
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(vpPos.x + 40.0f, vpPos.y + vpSize.y - 30.0f),
        IM_COL32(16, 13, 10, 180),
        "[W/S] NAVIGATE  [ENTER] EQUIP/UNEQUIP  [C] CONFIRM  [ESC] BACK");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
