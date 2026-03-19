/**
 * @file UI_MenuNav.h
 * @brief Shared vertical menu navigation logic (keyboard W/S + mouse hover + confirm).
 *
 * Extracts the repeated navigation pattern from MainMenu, PauseMenu, GameOver, Lobby.
 */
#pragma once
#ifdef USE_IMGUI

#include <imgui.h>
#include <cstdint>
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_UIKeyConfig.h"

namespace ECS::UI::Nav {

struct MenuNavResult {
    int8_t hoveredIndex   = -1;   ///< Mouse-hovered item (-1 if none)
    int8_t confirmedIndex = -1;   ///< Confirmed item (-1 if none)
};

/// @brief Process vertical menu navigation (keyboard up/down + mouse hover + confirm).
/// @param input        Current frame input state
/// @param uiCfg        Key binding config
/// @param selectedIndex [in/out] Current selected index (modified by keyboard)
/// @param itemCount    Total number of menu items
/// @param itemMins     Array of item min bounds (size >= itemCount)
/// @param itemMaxs     Array of item max bounds (size >= itemCount)
/// @return Navigation result with hovered and confirmed indices
inline MenuNavResult ProcessVerticalMenu(
    const Res_Input& input, const Res_UIKeyConfig& uiCfg,
    int8_t& selectedIndex, int itemCount,
    const ImVec2* itemMins, const ImVec2* itemMaxs)
{
    MenuNavResult result;

    // Keyboard navigation
    if (input.keyPressed[uiCfg.keyMenuUp] || input.keyPressed[uiCfg.keyMenuUpAlt]) {
        selectedIndex = (selectedIndex - 1 + itemCount) % itemCount;
    }
    if (input.keyPressed[uiCfg.keyMenuDown] || input.keyPressed[uiCfg.keyMenuDownAlt]) {
        selectedIndex = (selectedIndex + 1) % itemCount;
    }

    // Mouse hover
    ImVec2 mousePos = ImGui::GetMousePos();
    for (int i = 0; i < itemCount; ++i) {
        if (mousePos.x >= itemMins[i].x && mousePos.x <= itemMaxs[i].x &&
            mousePos.y >= itemMins[i].y && mousePos.y <= itemMaxs[i].y) {
            selectedIndex = static_cast<int8_t>(i);
            result.hoveredIndex = static_cast<int8_t>(i);
            break;
        }
    }

    // Keyboard confirm
    if (input.keyPressed[uiCfg.keyConfirm] || input.keyPressed[uiCfg.keyConfirmAlt]) {
        result.confirmedIndex = selectedIndex;
    }

    // Mouse confirm
    if (input.mouseButtonPressed[uiCfg.mouseConfirm]) {
        for (int i = 0; i < itemCount; ++i) {
            if (mousePos.x >= itemMins[i].x && mousePos.x <= itemMaxs[i].x &&
                mousePos.y >= itemMins[i].y && mousePos.y <= itemMaxs[i].y) {
                result.confirmedIndex = static_cast<int8_t>(i);
                break;
            }
        }
    }

    return result;
}

} // namespace ECS::UI::Nav

#endif // USE_IMGUI
