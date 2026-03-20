/**
 * @file UI_HUD_Degradation.cpp
 * @brief HUD sub-module: full-screen degradation overlay (noise dots + scan lines).
 */
#include "UI_HUD_Internal.h"
#ifdef USE_IMGUI

#include <cmath>
#include <algorithm>
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI::HUD {

/// @brief Render full-screen degradation overlay based on alert ratio.
///
/// Phase 0 (0-0.3): no effect. Phase 1 (0.3-0.6): noise dots with
/// LCG-based pseudo-random placement. Phase 2 (0.6-1.0): horizontal
/// glitch lines + scan lines. Intensity scales linearly within each phase.
void Degradation(ImDrawList* draw, const Res_GameState& gs,
                  float displayW, float displayH, float globalTime) {
    float alertMax = (gs.alertMax > 0.001f) ? gs.alertMax : 1.0f;
    float alertRatio = std::clamp(gs.alertLevel / alertMax, 0.0f, 1.0f);

    // Phase 0: 0~0.3 — no effect
    if (alertRatio < 0.3f) return;

    // Phase 1: 0.3~0.6 — noise dots
    if (alertRatio >= 0.3f) {
        float intensity = std::clamp((alertRatio - 0.3f) / 0.3f, 0.0f, 1.0f);
        int dotCount = (int)(intensity * 60);
        unsigned int seed = (unsigned int)(globalTime * 1000.0f);
        for (int i = 0; i < dotCount; ++i) {
            seed = seed * 1103515245u + 12345u;
            float nx = (float)(seed % (unsigned int)displayW);
            seed = seed * 1103515245u + 12345u;
            float ny = (float)(seed % (unsigned int)displayH);
            seed = seed * 1103515245u + 12345u;
            uint8_t a = (uint8_t)(20 + (seed % 30));
            draw->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + 2.0f, ny + 2.0f),
                Col32_BgDark(a));
        }
    }

    // Phase 2: 0.6~1.0 — horizontal glitch lines + scan lines
    if (alertRatio >= 0.6f) {
        float intensity = std::clamp((alertRatio - 0.6f) / 0.4f, 0.0f, 1.0f);

        // Horizontal glitch lines
        int lineCount = (int)(intensity * 8);
        unsigned int seed = (unsigned int)(globalTime * 500.0f);
        for (int i = 0; i < lineCount; ++i) {
            seed = seed * 1103515245u + 12345u;
            float ly = (float)(seed % (unsigned int)displayH);
            seed = seed * 1103515245u + 12345u;
            float lx = (float)(seed % (unsigned int)(displayW * 0.3f));
            seed = seed * 1103515245u + 12345u;
            float lw = 50.0f + (float)(seed % 200);
            uint8_t a = (uint8_t)(15 + intensity * 25);
            draw->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + lw, ly + 2.0f),
                Col32_Accent(a));
        }

        // Scan lines
        float spacing = 4.0f - intensity * 2.0f;
        uint8_t alpha = (uint8_t)(10 + intensity * 20);
        if (spacing < 2.0f) spacing = 2.0f;
        for (float y = 0.0f; y < displayH; y += spacing) {
            draw->AddLine(ImVec2(0.0f, y), ImVec2(displayW, y),
                Col32_BgDark(alpha), 1.0f);
        }
    }
}

} // namespace ECS::UI::HUD

#endif // USE_IMGUI
