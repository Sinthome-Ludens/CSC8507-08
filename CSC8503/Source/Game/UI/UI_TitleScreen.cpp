#include "UI_TitleScreen.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

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
        IM_COL32(245, 238, 232, 255));

    float cx = vpPos.x + vpSize.x * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.45f;

    // Vinyl record circles (decorative)
    float recordR = vpSize.y * 0.22f;
    draw->AddCircle(ImVec2(cx, cy), recordR,
        IM_COL32(200, 200, 200, 60), 64, 2.0f);
    draw->AddCircle(ImVec2(cx, cy), recordR * 0.7f,
        IM_COL32(200, 200, 200, 40), 48, 1.5f);
    draw->AddCircle(ImVec2(cx, cy), recordR * 0.4f,
        IM_COL32(200, 200, 200, 30), 32, 1.0f);
    draw->AddCircleFilled(ImVec2(cx, cy), recordR * 0.08f,
        IM_COL32(252, 111, 41, 120));

    // Rotating needle line
    float needleAngle = ui.titleTimer * 0.3f;
    float needleLen = recordR * 0.9f;
    float nx = cx + cosf(needleAngle) * needleLen;
    float ny = cy + sinf(needleAngle) * needleLen;
    draw->AddLine(ImVec2(cx, cy), ImVec2(nx, ny),
        IM_COL32(252, 111, 41, 80), 1.5f);

    // Game title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "NEUROMANCER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    float titleX = cx - titleSize.x * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.68f;

    // Shadow
    draw->AddText(ImVec2(titleX + 2, titleY + 2),
        IM_COL32(16, 13, 10, 40), title);
    // Main
    draw->AddText(ImVec2(titleX, titleY),
        IM_COL32(16, 13, 10, 255), title);

    if (titleFont) ImGui::PopFont();

    // Subtitle
    ImFont* bodyFont = UITheme::GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* subtitle = "TACTICAL NETWORK INFILTRATION";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    float subX = cx - subSize.x * 0.5f;
    float subY = titleY + titleSize.y + 8.0f;
    draw->AddText(ImVec2(subX, subY),
        IM_COL32(16, 13, 10, 200), subtitle);

    if (bodyFont) ImGui::PopFont();

    // Decorative line
    float lineY = subY + 28.0f;
    draw->AddLine(ImVec2(cx - 100.0f, lineY), ImVec2(cx + 100.0f, lineY),
        IM_COL32(200, 200, 200, 120), 1.0f);

    // Blinking prompt
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* prompt = "PRESS ANY KEY";
    float blinkAlpha = (sinf(ui.titleTimer * UITheme::kPI * 1.5f) + 1.0f) * 0.5f;
    uint8_t promptAlpha = (uint8_t)(std::min(blinkAlpha * 200.0f + 55.0f, 255.0f));
    ImVec2 promptSize = ImGui::CalcTextSize(prompt);
    float promptX = cx - promptSize.x * 0.5f;
    float promptY = lineY + 20.0f;
    draw->AddText(ImVec2(promptX, promptY),
        IM_COL32(252, 111, 41, promptAlpha), prompt);

    if (termFont) ImGui::PopFont();

    // Bottom credit
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    const char* credit = "TEAM 08 // CSC8507 // 2025";
    ImVec2 creditSize = ImGui::CalcTextSize(credit);
    draw->AddText(
        ImVec2(cx - creditSize.x * 0.5f, vpPos.y + vpSize.y - 35.0f),
        IM_COL32(16, 13, 10, 160), credit);

    if (smallFont) ImGui::PopFont();

    ImGui::End();

    // Detect any key/mouse → Splash
    if (ui.titleTimer > 0.5f) {
        bool anyInput = false;
        const Keyboard* kb = Window::GetKeyboard();
        if (kb) {
            for (int k = (int)KeyCodes::BACK; k < (int)KeyCodes::MAXVALUE; ++k) {
                if (kb->KeyPressed(static_cast<KeyCodes::Type>(k))) {
                    anyInput = true;
                    break;
                }
            }
        }
        if (!anyInput) {
            const Mouse* mouse = Window::GetMouse();
            if (mouse && (mouse->ButtonPressed(NCL::MouseButtons::Left) ||
                          mouse->ButtonPressed(NCL::MouseButtons::Right))) {
                anyInput = true;
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
