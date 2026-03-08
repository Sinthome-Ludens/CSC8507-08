/**
 * @file UI_Menus.h
 * @brief 核心菜单渲染：Splash、MainMenu、Settings、PauseMenu
 *
 * @note Called by Sys_UI::OnUpdate()
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"
#include "Game/Components/Res_UIState.h"

namespace ECS::UI {

// 共享导航辅助函数（Sys_UI ESC 和 Settings BACK 按钮共用）
void NavigateBackFromSettings(Res_UIState& ui);

void RenderSplashScreen  (Registry& registry, float dt);
void RenderMainMenu      (Registry& registry, float dt);
void RenderSettingsScreen(Registry& registry, float dt);
void RenderPauseMenu     (Registry& registry, float dt);

// Forward declarations for all UI modules (implementations in separate files)
// UI_TitleScreen.h  — RenderTitleScreen
// UI_HUD.h          — RenderHUD
// UI_GameOver.h     — RenderGameOverScreen
// UI_Team.h         — RenderTeamScreen
// UI_Loadout.h      — RenderLoadoutScreen
// UI_Inventory.h    — RenderInventoryScreen
// UI_Chat.h         — RenderChatPanel
// UI_ItemWheel.h    — RenderItemWheel
// UI_Interaction.h  — RenderInteractionPrompts
// UI_Effects.h      — RenderScanlineOverlay, RenderTransitionOverlay

} // namespace ECS::UI

#endif // USE_IMGUI
