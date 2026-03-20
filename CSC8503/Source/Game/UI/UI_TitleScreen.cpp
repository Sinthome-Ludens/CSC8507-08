/**
 * @file UI_TitleScreen.cpp
 * @brief 主标题/启动画面 UI。
 */
#include "UI_TitleScreen.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Input.h"

using namespace NCL;
using namespace ECS::UITheme;

namespace ECS::UI {

// ============================================================
// RenderTitleScreen — Record-player aesthetic title screen
// ============================================================

void RenderTitleScreen(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    ui.titleTimer += dt;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##TitleScreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background — warm cream #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_Bg(255));

    float cx = vpPos.x + vpSize.x * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.45f;

    // Vinyl record circles (decorative)
    float recordR = vpSize.y * 0.22f;
    draw->AddCircle(ImVec2(cx, cy), recordR,
        Col32_Gray(60), 64, 2.0f);
    draw->AddCircle(ImVec2(cx, cy), recordR * 0.7f,
        Col32_Gray(40), 48, 1.5f);
    draw->AddCircle(ImVec2(cx, cy), recordR * 0.4f,
        Col32_Gray(30), 32, 1.0f);
    draw->AddCircleFilled(ImVec2(cx, cy), recordR * 0.08f,
        Col32_Accent(120));

    // Record groove lines (dense arcs between main rings)
    for (int i = 1; i <= 6; ++i) {
        float t = (float)i / 7.0f;
        float grooveR = recordR * (0.4f + t * 0.6f);
        uint8_t grooveAlpha = (uint8_t)(12 + i * 3);
        draw->AddCircle(ImVec2(cx, cy), grooveR,
            Col32_Gray(grooveAlpha), 48, 0.5f);
    }

    // Outer ring with slow rotation (tick marks)
    {
        float outerR = recordR * 1.08f;
        float rotAngle = ui.titleTimer * 0.05f;
        constexpr int tickCount = 36;
        for (int i = 0; i < tickCount; ++i) {
            float a = rotAngle + (float)i * (2.0f * 3.14159f / tickCount);
            float inner = outerR - 4.0f;
            float outer = outerR;
            uint8_t tickAlpha = (i % 3 == 0) ? (uint8_t)50 : (uint8_t)25;
            draw->AddLine(
                ImVec2(cx + cosf(a) * inner, cy + sinf(a) * inner),
                ImVec2(cx + cosf(a) * outer, cy + sinf(a) * outer),
                Col32_Gray(tickAlpha), 1.0f);
        }
    }

    // Rotating needle line
    float needleAngle = ui.titleTimer * 0.3f;
    float needleLen = recordR * 0.9f;
    float nx = cx + cosf(needleAngle) * needleLen;
    float ny = cy + sinf(needleAngle) * needleLen;
    draw->AddLine(ImVec2(cx, cy), ImVec2(nx, ny),
        Col32_Accent(80), 1.5f);

    // Game title
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = cx - titleSize.x * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.68f;

    // Shadow
    draw->AddText(ImVec2(titleX + 2, titleY + 2),
        Col32_Text(40), title);
    // Main
    draw->AddText(ImVec2(titleX, titleY),
        Col32_Text(255), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* bodyFont = GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* subtitle = "TACTICAL NETWORK INFILTRATION";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    float subX = cx - subSize.x * 0.5f;
    float subY = titleY + titleSize.y + 8.0f;
    draw->AddText(ImVec2(subX, subY),
        Col32_Text(200), subtitle);

    if (bodyFont) ImGui::PopFont();

    // Decorative line
    float lineY = subY + 28.0f;
    draw->AddLine(ImVec2(cx - 100.0f, lineY), ImVec2(cx + 100.0f, lineY),
        Col32_Gray(120), 1.0f);

    // Blinking prompt
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* prompt = "PRESS ANY KEY";
    float blinkAlpha = (sinf(ui.titleTimer * kPI * 1.5f) + 1.0f) * 0.5f;
    uint8_t promptAlpha = (uint8_t)(std::min(blinkAlpha * 200.0f + 55.0f, 255.0f));
    ImVec2 promptSize = ImGui::CalcTextSize(prompt);
    float promptX = cx - promptSize.x * 0.5f;
    float promptY = lineY + 20.0f;
    draw->AddText(ImVec2(promptX, promptY),
        Col32_Accent(promptAlpha), prompt);

    if (termFont) ImGui::PopFont();

    // Bottom credit
    ImFont* smallFont = GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    const char* credit = "TEAM 08 // CSC8507 // 2025";
    ImVec2 creditSize = ImGui::CalcTextSize(credit);
    draw->AddText(
        ImVec2(cx - creditSize.x * 0.5f, vpPos.y + vpSize.y - 35.0f),
        Col32_Text(160), credit);

    if (smallFont) ImGui::PopFont();

    ImGui::End();

    // Detect any key/mouse → Splash
    if (ui.titleTimer > 0.5f) {
        bool anyInput = false;
        const auto& input = registry.ctx<Res_Input>();
        {
            for (int k = (int)KeyCodes::BACK; k < (int)KeyCodes::MAXVALUE; ++k) {
                if (input.keyPressed[static_cast<KeyCodes::Type>(k)]) {
                    anyInput = true;
                    break;
                }
            }
        }
        if (!anyInput) {
            for (size_t m = 0; m < std::size(input.mouseButtonPressed); ++m) {
                if (input.mouseButtonPressed[m]) { anyInput = true; break; }
            }
        }
        if (anyInput) {
            ui.previousScreen = ui.activeScreen;
            ui.activeScreen = UIScreen::Splash;
            ui.splashTimer = 0.0f;
            LOG_INFO("[UI_TitleScreen] TitleScreen -> Splash");
        }
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
