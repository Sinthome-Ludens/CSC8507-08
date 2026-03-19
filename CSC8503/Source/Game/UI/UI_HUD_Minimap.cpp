/**
 * @file UI_HUD_Minimap.cpp
 * @brief HUD sub-module: left-side minimap overlay (GlobalMap item active).
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

    // "MAP" title + countdown
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(kMapX + 4.0f, kMapY + 2.0f),
                  Col32_Accent(200), "MAP");

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
