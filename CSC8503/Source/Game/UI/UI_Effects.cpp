#include "UI_Effects.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

using namespace ECS::UITheme;

namespace ECS::UI {

// ============================================================
// RenderScanlineOverlay — CRT scanline effect
// ============================================================

void RenderScanlineOverlay(float globalTime) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    // Subtle horizontal scanlines
    constexpr float lineSpacing = 3.0f;
    constexpr uint8_t lineAlpha = 8;

    for (float y = 0.0f; y < displaySize.y; y += lineSpacing) {
        draw->AddLine(
            ImVec2(0.0f, y),
            ImVec2(displaySize.x, y),
            Col32_BgDark(lineAlpha), 1.0f);
    }

    // Moving scan bar
    float scanY = fmodf(globalTime * 60.0f, displaySize.y + 80.0f) - 40.0f;
    for (int i = 0; i < 20; ++i) {
        float y = scanY + (float)i * 2.0f;
        float t = 1.0f - fabsf((float)i - 10.0f) / 10.0f;
        uint8_t alpha = (uint8_t)(t * 12.0f);
        draw->AddLine(
            ImVec2(0.0f, y),
            ImVec2(displaySize.x, y),
            Col32_Bg(alpha), 1.0f);
    }
}

// ============================================================
// RenderVignetteOverlay — CRT edge darkening
// ============================================================

void RenderVignetteOverlay() {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 ds = ImGui::GetIO().DisplaySize;

    // Four gradient strips at edges (top/bottom/left/right)
    // Proportional: ~11% of the shorter dimension
    float edgeW = std::min(ds.x, ds.y) * 0.11f;
    if (edgeW < 60.0f) edgeW = 60.0f;
    constexpr uint8_t maxAlpha = 60;  // subtle darkening

    // Top edge
    for (int i = 0; i < (int)edgeW; i += 4) {
        float t = 1.0f - (float)i / edgeW;
        uint8_t a = (uint8_t)(t * t * maxAlpha);
        draw->AddRectFilled(
            ImVec2(0.0f, (float)i),
            ImVec2(ds.x, (float)(i + 4)),
            IM_COL32(0, 0, 0, a));
    }
    // Bottom edge
    for (int i = 0; i < (int)edgeW; i += 4) {
        float t = 1.0f - (float)i / edgeW;
        uint8_t a = (uint8_t)(t * t * maxAlpha);
        draw->AddRectFilled(
            ImVec2(0.0f, ds.y - (float)(i + 4)),
            ImVec2(ds.x, ds.y - (float)i),
            IM_COL32(0, 0, 0, a));
    }
    // Left edge
    for (int i = 0; i < (int)edgeW; i += 4) {
        float t = 1.0f - (float)i / edgeW;
        uint8_t a = (uint8_t)(t * t * maxAlpha);
        draw->AddRectFilled(
            ImVec2((float)i, 0.0f),
            ImVec2((float)(i + 4), ds.y),
            IM_COL32(0, 0, 0, a));
    }
    // Right edge
    for (int i = 0; i < (int)edgeW; i += 4) {
        float t = 1.0f - (float)i / edgeW;
        uint8_t a = (uint8_t)(t * t * maxAlpha);
        draw->AddRectFilled(
            ImVec2(ds.x - (float)(i + 4), 0.0f),
            ImVec2(ds.x - (float)i, ds.y),
            IM_COL32(0, 0, 0, a));
    }
}

// ============================================================
// RenderTransitionOverlay — CRT shrink/expand transition
// ============================================================

void RenderTransitionOverlay(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    if (!ui.transitionActive) return;

    ui.transitionTimer += dt;
    if (ui.transitionTimer >= ui.transitionDuration) {
        ui.transitionActive = false;
        ui.transitionTimer  = 0.0f;
        return;
    }

    float progress = std::clamp(ui.transitionTimer / ui.transitionDuration, 0.0f, 1.0f);

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    float cx = displaySize.x * 0.5f;
    float cy = displaySize.y * 0.5f;

    if (ui.transitionType == 0) {
        // FadeIn (CRT expand): black with horizontal line expanding outward
        float t = progress;
        // Phase 1 (0-0.4): full black with bright horizontal line
        // Phase 2 (0.4-1.0): vertical expand from center
        if (t < 0.4f) {
            // Full black
            draw->AddRectFilled(
                ImVec2(0.0f, 0.0f), ImVec2(displaySize.x, displaySize.y),
                Col32_BgDark());
            // Bright horizontal line
            float lineH = 2.0f + t * 5.0f;
            float lineAlpha = 200.0f + t * 55.0f;
            draw->AddRectFilled(
                ImVec2(0.0f, cy - lineH * 0.5f),
                ImVec2(displaySize.x, cy + lineH * 0.5f),
                Col32_Bg((uint8_t)lineAlpha));
        } else {
            // Vertical expand: reveal from center
            float expandT = (t - 0.4f) / 0.6f;  // 0->1
            float halfH = expandT * cy;

            // Top black bar
            if (cy - halfH > 0.0f) {
                draw->AddRectFilled(
                    ImVec2(0.0f, 0.0f),
                    ImVec2(displaySize.x, cy - halfH),
                    Col32_BgDark());
            }
            // Bottom black bar
            if (cy + halfH < displaySize.y) {
                draw->AddRectFilled(
                    ImVec2(0.0f, cy + halfH),
                    ImVec2(displaySize.x, displaySize.y),
                    Col32_BgDark());
            }

            // Edge glow at boundaries
            uint8_t edgeAlpha = (uint8_t)((1.0f - expandT) * 120.0f);
            if (edgeAlpha > 5) {
                draw->AddLine(
                    ImVec2(0.0f, cy - halfH),
                    ImVec2(displaySize.x, cy - halfH),
                    Col32_Bg(edgeAlpha), 2.0f);
                draw->AddLine(
                    ImVec2(0.0f, cy + halfH),
                    ImVec2(displaySize.x, cy + halfH),
                    Col32_Bg(edgeAlpha), 2.0f);
            }

            // Scanline distortion during expand
            float scanIntensity = (1.0f - expandT) * 0.5f;
            if (scanIntensity > 0.05f) {
                for (float y = cy - halfH; y < cy + halfH; y += 2.0f) {
                    uint8_t scanAlpha = (uint8_t)(scanIntensity * 30.0f);
                    draw->AddLine(
                        ImVec2(0.0f, y), ImVec2(displaySize.x, y),
                        Col32_Bg(scanAlpha), 1.0f);
                }
            }
        }
    } else {
        // FadeOut (CRT shrink): vertical collapse to center line, then black
        float t = progress;
        if (t < 0.6f) {
            // Vertical shrink: collapse toward center
            float shrinkT = t / 0.6f;  // 0->1
            float halfH = (1.0f - shrinkT) * cy;

            // Top black bar
            draw->AddRectFilled(
                ImVec2(0.0f, 0.0f),
                ImVec2(displaySize.x, cy - halfH),
                Col32_BgDark());
            // Bottom black bar
            draw->AddRectFilled(
                ImVec2(0.0f, cy + halfH),
                ImVec2(displaySize.x, displaySize.y),
                Col32_BgDark());

            // Edge glow
            uint8_t edgeAlpha = (uint8_t)(shrinkT * 150.0f);
            draw->AddLine(
                ImVec2(0.0f, cy - halfH),
                ImVec2(displaySize.x, cy - halfH),
                Col32_Bg(edgeAlpha), 2.0f);
            draw->AddLine(
                ImVec2(0.0f, cy + halfH),
                ImVec2(displaySize.x, cy + halfH),
                Col32_Bg(edgeAlpha), 2.0f);
        } else {
            // Full black with fading horizontal line
            draw->AddRectFilled(
                ImVec2(0.0f, 0.0f), ImVec2(displaySize.x, displaySize.y),
                Col32_BgDark());

            float fadeT = (t - 0.6f) / 0.4f;  // 0->1
            float lineH = 3.0f * (1.0f - fadeT);
            uint8_t lineAlpha = (uint8_t)(255.0f * (1.0f - fadeT));
            if (lineH > 0.2f && lineAlpha > 5) {
                draw->AddRectFilled(
                    ImVec2(0.0f, cy - lineH * 0.5f),
                    ImVec2(displaySize.x, cy + lineH * 0.5f),
                    Col32_Bg(lineAlpha));
            }
        }
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
