/**
 * @file UI_HUD_Internal.h
 * @brief HUD sub-module function declarations (internal, not part of public API).
 *
 * Each sub-module renders one logical section of the HUD.
 * Included by UI_HUD.cpp and all UI_HUD_*.cpp sub-modules.
 */
#pragma once
#ifdef USE_IMGUI

#include <imgui.h>
#include "Core/ECS/Registry.h"

// Forward declarations — avoid pulling heavy headers into every sub-module
struct Res_GameState;
namespace ECS { struct Res_UIState; }

namespace ECS::UI::HUD {

/// Left-top mission panel (mission name + objective).
void MissionPanel(ImDrawList* draw, const Res_GameState& gs, float gameW);

/// Right-top 4-segment alert gauge.
void AlertGauge(ImDrawList* draw, const Res_GameState& gs, float gameW);

/// Score + rating below alert gauge.
void Score(ImDrawList* draw, int32_t score, float gameW);

/// Center-top countdown timer.
void Countdown(ImDrawList* draw, const Res_GameState& gs, float gameW, float globalTime);

/// Left-bottom player state tags (STAND/CROUCH/RUN + DISGUISED).
void PlayerState(ImDrawList* draw, const Res_GameState& gs, float displayH);

/// Left-bottom noise concentric rings indicator.
void NoiseIndicator(ImDrawList* draw, const Res_GameState& gs, float displayH, float globalTime);

/// Bottom-center dual equipment panels (Q gadget + E weapon).
void ItemSlots(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH);

/// Full-screen degradation overlay (noise dots + scan lines).
void Degradation(ImDrawList* draw, const Res_GameState& gs, float displayW, float displayH, float globalTime);

/// Left-side minimap overlay (when RadarMap item is active).
void Minimap(ImDrawList* draw, Registry& registry, float displayH);

/// Multiplayer match banner (waiting / start).
void MatchBanner(ImDrawList* draw, const Res_GameState& gs, Res_UIState& ui, float gameW, float globalTime, float dt);

/// Multiplayer opponent progress bar.
void OpponentBar(ImDrawList* draw, const Res_GameState& gs, float gameW);

/// Multiplayer disruption full-screen effect.
void DisruptionEffect(ImDrawList* draw, const Res_GameState& gs, float displayW, float displayH, float globalTime);

/// Multiplayer network ping display.
void NetworkStatus(ImDrawList* draw, const Res_GameState& gs, float gameW, float displayH);

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
