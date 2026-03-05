#include "UI_Loadout.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

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

void RenderLoadoutScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

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
    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.loadoutSelectedIndex = (ui.loadoutSelectedIndex - 1 + kTotalEntries) % kTotalEntries;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
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

    // Column header: ITEMS
    if (termFont) ImGui::PushFont(termFont);
    draw->AddText(ImVec2(leftX, startY), IM_COL32(252, 111, 41, 220), "ITEMS");
    // Column header: WEAPONS
    draw->AddText(ImVec2(rightX, startY), IM_COL32(252, 111, 41, 220), "WEAPONS");
    if (termFont) ImGui::PopFont();

    float entryStartY = startY + 30.0f;
    float entryH = 50.0f;

    // Mouse hover + click
    const Mouse* mouse = Window::GetMouse();
    int8_t confirmedIndex = -1;

    auto drawColumn = [&](const LoadoutEntry* entries, int count, int indexOffset,
                          float colX, float colWidth) {
        for (int i = 0; i < count; ++i) {
            int globalIdx = indexOffset + i;
            float itemY = entryStartY + i * entryH;
            bool isSelected = (globalIdx == ui.loadoutSelectedIndex);

            ImVec2 itemMin(colX - 5.0f, itemY - 4.0f);
            ImVec2 itemMax(colX + colWidth, itemY + entryH - 8.0f);

            // Mouse hover
            if (mouse) {
                ImVec2 mousePos = ImGui::GetMousePos();
                if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                    mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                    ui.loadoutSelectedIndex = static_cast<int8_t>(globalIdx);
                    isSelected = true;
                }
            }

            if (isSelected) {
                draw->AddRectFilled(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 25), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);
            }

            // Name
            if (termFont) ImGui::PushFont(termFont);
            char nameBuf[64];
            snprintf(nameBuf, sizeof(nameBuf), isSelected ? "> %s" : "  %s",
                entries[i].name);
            draw->AddText(ImVec2(colX + (isSelected ? 4.0f : 0.0f), itemY),
                isSelected ? IM_COL32(16, 13, 10, 255) : IM_COL32(16, 13, 10, 220),
                nameBuf);
            if (termFont) ImGui::PopFont();

            // Description
            if (smallFont) ImGui::PushFont(smallFont);
            draw->AddText(ImVec2(colX + 18.0f, itemY + 20.0f),
                IM_COL32(16, 13, 10, 150), entries[i].description);
            if (smallFont) ImGui::PopFont();
        }
    };

    drawColumn(kItems, kItemCount, 0, leftX, colW);
    drawColumn(kWeapons, kWeaponCount, kItemCount, rightX, colW);

    // Confirm with ENTER
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.loadoutSelectedIndex;
    }
    // Mouse click confirm
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        confirmedIndex = ui.loadoutSelectedIndex;
    }

    if (confirmedIndex >= 0) {
        LOG_INFO("[UI_Loadout] Selected loadout index: " << (int)confirmedIndex);
    }

    // Divider between columns
    float divX = leftX + colW + 15.0f;
    draw->AddLine(
        ImVec2(divX, startY),
        ImVec2(divX, entryStartY + kItemCount * entryH),
        IM_COL32(200, 200, 200, 80), 1.0f);

    // Bottom hint
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(vpPos.x + 40.0f, vpPos.y + vpSize.y - 30.0f),
        IM_COL32(16, 13, 10, 180),
        "[W/S] NAVIGATE  [ENTER] SELECT  [ESC] BACK");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
