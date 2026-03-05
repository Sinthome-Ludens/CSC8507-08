#include "UI_GameOver.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// RenderGameOverScreen
// ============================================================

static const char* kGameOverItems[] = {
    "RETRY",
    "RETURN TO MENU",
};
static constexpr int kGameOverItemCount = 2;

void RenderGameOverScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##GameOver", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    float cx = vpPos.x + vpSize.x * 0.5f;

    // Get game data
    uint32_t score = 0;
    uint32_t enemies = 0;
    if (registry.has_ctx<Res_GameState>()) {
        auto& gs = registry.ctx<Res_GameState>();
        score   = gs.score;
        enemies = gs.enemyCount;
    }

    // Result title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* resultTitle = "MISSION COMPLETE";
    ImVec2 titleSize = ImGui::CalcTextSize(resultTitle);
    float titleX = cx - titleSize.x * 0.5f;
    float titleY = vpPos.y + vpSize.y * 0.15f;
    draw->AddText(ImVec2(titleX, titleY),
        IM_COL32(16, 13, 10, 255), resultTitle);

    if (titleFont) ImGui::PopFont();

    // Decorative line
    float lineY = titleY + titleSize.y + 12.0f;
    draw->AddLine(ImVec2(cx - 120.0f, lineY), ImVec2(cx + 120.0f, lineY),
        IM_COL32(200, 200, 200, 120), 1.0f);

    // Rating
    ImFont* bodyFont = UITheme::GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* rating = (score >= 500) ? "RATING: S" :
                         (score >= 300) ? "RATING: A" :
                         (score >= 100) ? "RATING: B" : "RATING: C";
    ImVec2 ratingSize = ImGui::CalcTextSize(rating);
    float ratingY = lineY + 20.0f;
    draw->AddText(ImVec2(cx - ratingSize.x * 0.5f, ratingY),
        IM_COL32(252, 111, 41, 255), rating);

    if (bodyFont) ImGui::PopFont();

    // Statistics panel
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float statsY = ratingY + 50.0f;
    float statsX = cx - 100.0f;

    char buf[64];
    snprintf(buf, sizeof(buf), "SCORE:     %u", score);
    draw->AddText(ImVec2(statsX, statsY),
        IM_COL32(16, 13, 10, 220), buf);

    snprintf(buf, sizeof(buf), "HOSTILES:  %u", enemies);
    draw->AddText(ImVec2(statsX, statsY + 28.0f),
        IM_COL32(16, 13, 10, 220), buf);

    draw->AddText(ImVec2(statsX, statsY + 56.0f),
        IM_COL32(16, 13, 10, 220), "DETECTED:  NO");

    if (termFont) ImGui::PopFont();

    // Separator before menu
    float sepY = statsY + 100.0f;
    draw->AddLine(ImVec2(cx - 80.0f, sepY), ImVec2(cx + 80.0f, sepY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // Menu items
    if (termFont) ImGui::PushFont(termFont);

    float menuStartY = sepY + 20.0f;
    float menuItemH  = 38.0f;

    const Keyboard* kb = Window::GetKeyboard();
    if (kb) {
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            ui.gameOverSelectedIndex = (ui.gameOverSelectedIndex - 1 + kGameOverItemCount) % kGameOverItemCount;
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            ui.gameOverSelectedIndex = (ui.gameOverSelectedIndex + 1) % kGameOverItemCount;
        }
    }

    const Mouse* mouse = Window::GetMouse();
    float itemW = 240.0f;
    float itemStartX = cx - itemW * 0.5f;

    for (int i = 0; i < kGameOverItemCount; ++i) {
        ImVec2 itemMin(itemStartX, menuStartY + i * menuItemH - 2.0f);
        ImVec2 itemMax(itemStartX + itemW, menuStartY + i * menuItemH + menuItemH - 6.0f);
        if (mouse) {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                ui.gameOverSelectedIndex = static_cast<int8_t>(i);
            }
        }
    }

    int8_t confirmedIndex = -1;
    if (kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE))) {
        confirmedIndex = ui.gameOverSelectedIndex;
    }
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        for (int i = 0; i < kGameOverItemCount; ++i) {
            ImVec2 itemMin(itemStartX, menuStartY + i * menuItemH - 2.0f);
            ImVec2 itemMax(itemStartX + itemW, menuStartY + i * menuItemH + menuItemH - 6.0f);
            if (mousePos.x >= itemMin.x && mousePos.x <= itemMax.x &&
                mousePos.y >= itemMin.y && mousePos.y <= itemMax.y) {
                confirmedIndex = static_cast<int8_t>(i);
                break;
            }
        }
    }

    for (int i = 0; i < kGameOverItemCount; ++i) {
        float itemY = menuStartY + i * menuItemH;
        bool isSelected = (i == ui.gameOverSelectedIndex);

        ImVec2 itemMin(itemStartX, itemY - 2.0f);
        ImVec2 itemMax(itemStartX + itemW, itemY + menuItemH - 6.0f);

        if (isSelected) {
            draw->AddRectFilled(itemMin, itemMax,
                IM_COL32(252, 111, 41, 25), 2.0f);
            draw->AddRect(itemMin, itemMax,
                IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);
        }

        char label[64];
        snprintf(label, sizeof(label), isSelected ? "> %s" : "  %s",
            kGameOverItems[i]);
        ImU32 textColor = isSelected ? IM_COL32(16, 13, 10, 255)
                                     : IM_COL32(16, 13, 10, 220);
        float textX = itemStartX + 10.0f + (isSelected ? 4.0f : 0.0f);
        draw->AddText(ImVec2(textX, itemY), textColor, label);
    }

    if (termFont) ImGui::PopFont();

    // Confirm action
    if (confirmedIndex >= 0) {
        switch (confirmedIndex) {
            case 0: // RETRY
                ui.pendingSceneRequest = SceneRequest::RestartLevel;
                LOG_INFO("[UI_GameOver] GameOver -> Retry");
                break;
            case 1: // RETURN TO MENU
                ui.pendingSceneRequest = SceneRequest::ReturnToMenu;
                LOG_INFO("[UI_GameOver] GameOver -> ReturnToMenu");
                break;
            default:
                break;
        }
    }

    // Bottom hint
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    ImVec2 hintPos(cx - 120.0f, vpPos.y + vpSize.y - 35.0f);
    draw->AddText(hintPos, IM_COL32(16, 13, 10, 180),
        "[W/S] NAVIGATE  [ENTER] SELECT");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
