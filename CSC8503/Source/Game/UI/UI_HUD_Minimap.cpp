/**
 * @file UI_HUD_Minimap.cpp
 * @brief HUD sub-module: left-side minimap overlay (RadarMap item active).
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_MinimapState.h"
#include "Game/Components/Res_RadarState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

/// @brief Render left-side minimap overlay (active when RadarMap item is in use).
///
/// Maps world coordinates to a 160x160 screen panel via uniform scaling.
/// Draws walkable triangles, boundary edges, 4x4 grid, enemy dots (red),
/// player triangle (orange), compass labels, and remaining timer.
void Minimap(ImDrawList* draw, Registry& registry, float /*displayH*/) {
    if (!registry.has_ctx<Res_MinimapState>()) return;
    const auto& minimap = registry.ctx<Res_MinimapState>();
    if (!minimap.isActive || minimap.edgeCount == 0) return;

    // Layout
    constexpr float kMapX = 16.0f;
    constexpr float kMapY = 76.0f;
    constexpr float kMapSize = 160.0f;
    constexpr float kPadding = 8.0f;

    // Dark background
    draw->AddRectFilled(
        ImVec2(kMapX, kMapY),
        ImVec2(kMapX + kMapSize, kMapY + kMapSize),
        Col32_BgDark(180), Layout::kPanelRounding);

    // Border
    draw->AddRect(
        ImVec2(kMapX, kMapY),
        ImVec2(kMapX + kMapSize, kMapY + kMapSize),
        Col32_Accent(120), Layout::kPanelRounding);

    // World → screen mapping
    float worldW = minimap.worldMaxX - minimap.worldMinX;
    float worldH = minimap.worldMaxZ - minimap.worldMinZ;
    if (worldW < 0.1f || worldH < 0.1f) return;

    float drawSize = kMapSize - kPadding * 2;
    float scale = drawSize / std::max(worldW, worldH);
    float offsetX = kMapX + kPadding + (drawSize - worldW * scale) * 0.5f;
    float offsetZ = kMapY + kPadding + (drawSize - worldH * scale) * 0.5f;

    auto toScreen = [&](float wx, float wz) -> ImVec2 {
        return ImVec2(
            offsetX + (wx - minimap.worldMinX) * scale,
            offsetZ + (wz - minimap.worldMinZ) * scale);
    };

    // Grid lines (4x4)
    constexpr int kGridDiv = 4;
    for (int g = 1; g < kGridDiv; ++g) {
        float t = (float)g / kGridDiv;
        float gx = kMapX + kPadding + drawSize * t;
        float gy = kMapY + kPadding + drawSize * t;
        draw->AddLine(ImVec2(gx, kMapY + kPadding), ImVec2(gx, kMapY + kPadding + drawSize),
            Col32_Gray(25), 1.0f);
        draw->AddLine(ImVec2(kMapX + kPadding, gy), ImVec2(kMapX + kPadding + drawSize, gy),
            Col32_Gray(25), 1.0f);
    }

    // Walkable area fill (terrain color, not themed)
    for (int t = 0; t < minimap.triangleCount; ++t) {
        const auto& tri = minimap.triangles[t];
        ImVec2 a = toScreen(tri.x0, tri.z0);
        ImVec2 b = toScreen(tri.x1, tri.z1);
        ImVec2 c = toScreen(tri.x2, tri.z2);
        draw->AddTriangleFilled(a, b, c, IM_COL32(60, 55, 50, 100));
    }

    // Boundary edges
    for (int i = 0; i < minimap.edgeCount; ++i) {
        const auto& e = minimap.edges[i];
        ImVec2 a = toScreen(e.x0, e.z0);
        ImVec2 b = toScreen(e.x1, e.z1);
        draw->AddLine(a, b, Col32_Gray(140), 1.0f);
    }

    // Enemy positions (red dots, game mechanic)
    if (registry.has_ctx<Res_RadarState>()) {
        const auto& radar = registry.ctx<Res_RadarState>();
        if (radar.isActive) {
            for (int i = 0; i < radar.contactCount && i < Res_RadarState::kMaxContacts; ++i) {
                if (!radar.contacts[i].valid) continue;
                ImVec2 ep = toScreen(radar.contacts[i].worldPos.x,
                                      radar.contacts[i].worldPos.z);
                draw->AddCircleFilled(ep, 3.0f, IM_COL32(220, 60, 40, 220));
            }
        }
    }

    // Player position (orange triangle)
    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID, C_T_Player&, C_D_Transform& tf) {
            ImVec2 pp = toScreen(tf.position.x, tf.position.z);
            draw->AddTriangleFilled(
                ImVec2(pp.x, pp.y - 4.0f),
                ImVec2(pp.x - 3.0f, pp.y + 3.0f),
                ImVec2(pp.x + 3.0f, pp.y + 3.0f),
                Col32_Accent(255));
        });

    // Compass labels (N/S/E/W)
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    float mapCx = kMapX + kMapSize * 0.5f;
    float mapCy = kMapY + kMapSize * 0.5f;
    ImU32 compassCol = Col32_Bg(100);
    draw->AddText(ImVec2(mapCx - 3.0f, kMapY + 2.0f), Col32_Accent(140), "N");
    draw->AddText(ImVec2(mapCx - 3.0f, kMapY + kMapSize - 14.0f), compassCol, "S");
    draw->AddText(ImVec2(kMapX + 2.0f, mapCy - 6.0f), compassCol, "W");
    draw->AddText(ImVec2(kMapX + kMapSize - 10.0f, mapCy - 6.0f), compassCol, "E");
    if (smallFont) ImGui::PopFont();

    // "MAP" title + countdown
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(kMapX + 4.0f, kMapY + 2.0f),
                  Col32_Accent(200), "RADAR");

    if (minimap.activeTimer > 0.0f) {
        char timerBuf[8];
        snprintf(timerBuf, sizeof(timerBuf), "%.0fs", minimap.activeTimer);
        ImVec2 timerSize = ImGui::CalcTextSize(timerBuf);
        draw->AddText(ImVec2(kMapX + kMapSize - timerSize.x - 4.0f, kMapY + 2.0f),
                      Col32_Bg(200), timerBuf);
    }
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
