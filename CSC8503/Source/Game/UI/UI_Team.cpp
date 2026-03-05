#include "UI_Team.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// RenderTeamScreen — Typewriter-animated team credits
// ============================================================

struct TeamMember {
    const char* name;
    const char* role;
};

static const TeamMember kTeamMembers[] = {
    {"MEMBER 1",  "LEAD PROGRAMMER"},
    {"MEMBER 2",  "GAMEPLAY PROGRAMMER"},
    {"MEMBER 3",  "GRAPHICS PROGRAMMER"},
    {"MEMBER 4",  "AI PROGRAMMER"},
    {"MEMBER 5",  "NETWORK PROGRAMMER"},
    {"MEMBER 6",  "PHYSICS PROGRAMMER"},
    {"MEMBER 7",  "UI PROGRAMMER"},
    {"MEMBER 8",  "AUDIO PROGRAMMER"},
};
static constexpr int kTeamMemberCount = 8;

void RenderTeamScreen(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    ui.teamStartTime += dt;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##TeamScreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background — warm cream
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    float cx = vpPos.x + vpSize.x * 0.5f;

    // Title
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    const char* title = "THE TEAM";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    draw->AddText(ImVec2(cx - titleSize.x * 0.5f, vpPos.y + 40.0f),
        IM_COL32(16, 13, 10, 255), title);

    if (titleFont) ImGui::PopFont();

    // Decorative line
    float lineY = vpPos.y + 80.0f;
    draw->AddLine(ImVec2(cx - 100.0f, lineY), ImVec2(cx + 100.0f, lineY),
        IM_COL32(200, 200, 200, 120), 1.0f);

    // Team members with typewriter reveal
    ImFont* termFont = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    float memberStartY = lineY + 30.0f;
    float memberSpacing = 50.0f;
    float revealDelay = 0.3f;  // seconds between each member reveal
    float charDelay   = 0.03f; // seconds per character typewriter

    for (int i = 0; i < kTeamMemberCount; ++i) {
        float memberTime = ui.teamStartTime - revealDelay * (float)i;
        if (memberTime < 0.0f) continue;

        float itemY = memberStartY + i * memberSpacing;

        // Name (typewriter effect)
        if (termFont) ImGui::PushFont(termFont);
        {
            const char* name = kTeamMembers[i].name;
            int nameLen = (int)strlen(name);
            int visibleChars = std::min((int)(memberTime / charDelay), nameLen);

            char nameBuf[64] = {};
            for (int c = 0; c < visibleChars && c < 63; ++c) {
                nameBuf[c] = name[c];
            }
            nameBuf[visibleChars] = '\0';

            ImVec2 nameSize = ImGui::CalcTextSize(nameBuf);
            draw->AddText(ImVec2(cx - nameSize.x * 0.5f, itemY),
                IM_COL32(16, 13, 10, 255), nameBuf);

            // Blinking cursor at end
            if (visibleChars < nameLen) {
                float cursorBlink = sinf(ui.teamStartTime * UITheme::kPI * 4.0f);
                if (cursorBlink > 0.0f) {
                    ImVec2 cursorPos(cx - nameSize.x * 0.5f + nameSize.x, itemY);
                    draw->AddText(cursorPos, IM_COL32(252, 111, 41, 200), "_");
                }
            }
        }
        if (termFont) ImGui::PopFont();

        // Role (appears after name is fully typed)
        float nameFullTime = charDelay * (float)strlen(kTeamMembers[i].name);
        if (memberTime > nameFullTime + 0.2f) {
            if (smallFont) ImGui::PushFont(smallFont);
            float roleAlpha = std::min((memberTime - nameFullTime - 0.2f) * 4.0f, 1.0f);
            uint8_t ra = (uint8_t)(roleAlpha * 180.0f);

            const char* role = kTeamMembers[i].role;
            ImVec2 roleSize = ImGui::CalcTextSize(role);
            draw->AddText(ImVec2(cx - roleSize.x * 0.5f, itemY + 22.0f),
                IM_COL32(16, 13, 10, ra), role);
            if (smallFont) ImGui::PopFont();
        }
    }

    // Bottom hint
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(vpPos.x + 30.0f, vpPos.y + vpSize.y - 30.0f),
        IM_COL32(16, 13, 10, 160), "[ESC] BACK");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
