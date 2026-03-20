/**
 * @file UI_HUD.cpp
 * @brief HUD 渲染入口：调度所有子模块面板渲染。
 *
 * @details
 * 子面板拆分至 UI_HUD_*.cpp（Mission/Alert/Status/Equipment/Minimap/
 * Multiplayer/Degradation），此文件仅保留 RenderHUD() 入口调度逻辑。
 *
 * @see UI_HUD.h, UI_HUD_Internal.h
 */
#include "UI_HUD.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_HUD_Internal.h"

using namespace ECS::UITheme;

namespace ECS::UI {

/// @brief 返回去掉右侧聊天面板后的游戏区域宽度（像素）。
static float GetGameAreaWidth(float displayW) {
    return displayW - Res_ChatState::kPanelWidth;
}

/**
 * @brief HUD 渲染入口：按顺序调度所有子面板。
 *        由 Sys_UI::OnUpdate 在 UIScreen::HUD 状态下调用。
 * @param registry ECS 注册表（读取 Res_UIState、Res_GameState）
 * @param dt       帧时间（秒）
 */
void RenderHUD(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_GameState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    const auto& gs = registry.ctx<Res_GameState>();

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float gameW = GetGameAreaWidth(displaySize.x);
    float displayH = displaySize.y;

    // ── Single-player & shared panels ──
    HUD::MissionPanel(draw, gs, gameW);
    HUD::AlertGauge(draw, gs, gameW);
    HUD::Score(draw, ui.campaignScore, gameW);
    HUD::Countdown(draw, gs, gameW, ui.globalTime);
    HUD::PlayerState(draw, gs, displayH);
    HUD::NoiseIndicator(draw, gs, displayH, ui.globalTime);
    HUD::ItemSlots(draw, gs, gameW, displayH);
    HUD::Degradation(draw, gs, displaySize.x, displayH, ui.globalTime);
    HUD::Minimap(draw, registry, displayH);

    // ── Multiplayer-only panels ──
    if (gs.isMultiplayer) {
        HUD::MatchBanner(draw, gs, ui, gameW, ui.globalTime, dt);
        HUD::OpponentBar(draw, gs, gameW);
        HUD::DisruptionEffect(draw, gs, displaySize.x, displayH, ui.globalTime);
        HUD::NetworkStatus(draw, gs, gameW, displayH);
    }

    // ── Control hints ──
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    const char* hints = "[ESC] PAUSE  [Q] GADGET  [E] WEAPON  [TAB] SWITCH  [I] INVENTORY";
    ImVec2 hintsSize = ImGui::CalcTextSize(hints);
    draw->AddText(
        ImVec2(gameW * 0.5f - hintsSize.x * 0.5f, displayH - 18.0f),
        Col32_Bg(100), hints);
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
