#include "UI_Loading.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {
using namespace ECS::UITheme;

// 系统消息：赛博朋克风格的加载提示
static constexpr const char* kLoadingMessages[] = {
    "ESTABLISHING SECURE TUNNEL...",
    "DECRYPTING FIREWALL PROTOCOLS...",
    "LOADING MISSION PARAMETERS...",
    "CALIBRATING NEURAL INTERFACE...",
    "SYNCHRONIZING FIELD ASSETS...",
    "MAPPING OPERATIONAL ZONE...",
    "INJECTING PAYLOAD MODULES...",
    "VERIFYING OPERATOR CLEARANCE...",
};
static constexpr int kLoadingMsgCount = 8;
static constexpr float kMsgInterval   = 0.35f;  // 消息轮播间隔（秒）

// ============================================================
// RenderLoadingScreen
// ============================================================

void RenderLoadingScreen(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    ui.loadingTimer += dt;

    // 消息轮播
    ui.loadingMsgTimer += dt;
    if (ui.loadingMsgTimer >= kMsgInterval && ui.loadingMsgIndex < kLoadingMsgCount - 1) {
        ui.loadingMsgTimer -= kMsgInterval;
        ui.loadingMsgIndex++;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##LoadingScreen", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background — near-black (#100D0A)
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        Col32_BgDark());

    float cx = vpPos.x + vpSize.x * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.5f;

    // ── Background geometric pattern (subtle hex grid) ────────
    {
        constexpr float hexSpacing = 60.0f;
        constexpr float hexR = 20.0f;
        ImU32 hexCol = Col32_Bg(8);
        for (float hx = vpPos.x; hx < vpPos.x + vpSize.x + hexSpacing; hx += hexSpacing) {
            for (float hy = vpPos.y; hy < vpPos.y + vpSize.y + hexSpacing; hy += hexSpacing * 0.866f) {
                float ox = (int(hy / (hexSpacing * 0.866f)) % 2 == 0) ? 0.0f : hexSpacing * 0.5f;
                float px = hx + ox;
                // Draw hexagon outline
                for (int s = 0; s < 6; ++s) {
                    float a0 = (float)s * 1.0472f;       // pi/3
                    float a1 = (float)(s + 1) * 1.0472f;
                    draw->AddLine(
                        ImVec2(px + cosf(a0) * hexR, hy + sinf(a0) * hexR),
                        ImVec2(px + cosf(a1) * hexR, hy + sinf(a1) * hexR),
                        hexCol, 1.0f);
                }
            }
        }
    }

    // ── Decorative rotating ring ──────────────────────────────
    float ringR = 40.0f;
    float ringCY = cy - 60.0f;

    // Outer ring (dim)
    draw->AddCircle(ImVec2(cx, ringCY), ringR,
        Col32_Bg(30), 48, 1.5f);

    // Rotating arc segments (orange)
    float angle = ui.loadingTimer * 2.5f;
    for (int i = 0; i < 3; ++i) {
        float a0 = angle + (float)i * (kPI * 2.0f / 3.0f);
        float a1 = a0 + 0.8f;
        constexpr int arcSegs = 12;
        for (int s = 0; s < arcSegs; ++s) {
            float t0 = a0 + (a1 - a0) * (float)s / (float)arcSegs;
            float t1 = a0 + (a1 - a0) * (float)(s + 1) / (float)arcSegs;
            draw->AddLine(
                ImVec2(cx + cosf(t0) * ringR, ringCY + sinf(t0) * ringR),
                ImVec2(cx + cosf(t1) * ringR, ringCY + sinf(t1) * ringR),
                Col32_Accent(200), 2.0f);
        }
    }

    // Outer pulsing ring (breathe scale)
    float breathe = (sinf(ui.loadingTimer * 1.5f) + 1.0f) * 0.5f;
    float pulseR = ringR * (1.05f + breathe * 0.08f);
    draw->AddCircle(ImVec2(cx, ringCY), pulseR,
        Col32_Accent((uint8_t)(20 + breathe * 30)), 48, 1.0f);

    // Inner dot (pulsing)
    float pulse = (sinf(ui.loadingTimer * kPI * 2.0f) + 1.0f) * 0.5f;
    uint8_t dotAlpha = (uint8_t)(120.0f + pulse * 135.0f);
    draw->AddCircleFilled(ImVec2(cx, ringCY), 4.0f,
        Col32_Accent(dotAlpha));

    // ── "LOADING" title ───────────────────────────────────────
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    // Animated dots
    int dotCount = ((int)(ui.loadingTimer * 3.0f)) % 4;
    char loadingText[16];
    const char* dots[] = { "LOADING", "LOADING.", "LOADING..", "LOADING..." };
    // safe copy
    const char* src = dots[dotCount];
    int ci = 0;
    while (src[ci] && ci < 15) { loadingText[ci] = src[ci]; ci++; }
    loadingText[ci] = '\0';

    ImVec2 loadSize = ImGui::CalcTextSize(loadingText);
    float loadX = cx - loadSize.x * 0.5f;
    float loadY = ringCY + ringR + 24.0f;

    draw->AddText(ImVec2(loadX, loadY),
        Col32_Bg(), loadingText);

    if (titleFont) ImGui::PopFont();

    // ── Progress bar ──────────────────────────────────────────
    ImFont* termFont = GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    float barW = vpSize.x * 0.4f;
    float barH = 4.0f;
    float barX = cx - barW * 0.5f;
    float barY = loadY + loadSize.y + 20.0f;

    // Progress: ramp up over loadingMinDuration
    float progress = ui.loadingTimer / ui.loadingMinDuration;
    if (progress > 1.0f) progress = 1.0f;

    // Bar background
    draw->AddRectFilled(
        ImVec2(barX, barY),
        ImVec2(barX + barW, barY + barH),
        Col32_Bg(30));

    // Bar fill (orange)
    draw->AddRectFilled(
        ImVec2(barX, barY),
        ImVec2(barX + barW * progress, barY + barH),
        Col32_Accent(220));

    // Percentage
    char pctText[8];
    int pct = (int)(progress * 100.0f);
    snprintf(pctText, sizeof(pctText), "%d%%", pct);
    const char* pctDisplay = pctText;

    ImVec2 pctSize = ImGui::CalcTextSize(pctDisplay);
    draw->AddText(
        ImVec2(barX + barW + 12.0f, barY - pctSize.y * 0.5f + barH * 0.5f),
        Col32_Bg(180), pctDisplay);

    // ── System messages (scrolling log) ───────────────────────
    float msgY = barY + barH + 30.0f;
    ImFont* smallFont = GetFont_Small();
    if (smallFont) {
        if (termFont) ImGui::PopFont();  // pop termFont only if it was pushed
        ImGui::PushFont(smallFont);
    }

    for (int i = 0; i <= (int)ui.loadingMsgIndex && i < kLoadingMsgCount; ++i) {
        // Fade: most recent message is brightest
        int distance = (int)ui.loadingMsgIndex - i;
        uint8_t alpha = 180;
        if (distance == 1) alpha = 100;
        else if (distance == 2) alpha = 50;
        else if (distance >= 3) alpha = 25;

        // Slide-up: newest message slides in from below
        float slideY = 0.0f;
        if (i == (int)ui.loadingMsgIndex && ui.loadingMsgTimer < kMsgInterval) {
            float t = ui.loadingMsgTimer / kMsgInterval;
            slideY = (1.0f - t) * 10.0f;
            alpha = (uint8_t)(alpha * t);
        }

        // Prefix "> "
        char msgBuf[64];
        msgBuf[0] = '>';
        msgBuf[1] = ' ';
        const char* msg = kLoadingMessages[i];
        int mi = 0;
        while (msg[mi] && (mi + 2) < 63) { msgBuf[mi + 2] = msg[mi]; mi++; }
        msgBuf[mi + 2] = '\0';

        draw->AddText(
            ImVec2(barX, msgY + (float)i * 18.0f + slideY),
            Col32_Bg(alpha), msgBuf);
    }

    if (smallFont) ImGui::PopFont();
    else if (termFont) ImGui::PopFont();

    // ── Bottom hint ───────────────────────────────────────────
    ImFont* bodyFont = GetFont_Body();
    if (bodyFont) ImGui::PushFont(bodyFont);

    const char* hint = "TEAM 08 // NEUROMANCER";
    ImVec2 hintSize = ImGui::CalcTextSize(hint);
    draw->AddText(
        ImVec2(cx - hintSize.x * 0.5f, vpPos.y + vpSize.y - 35.0f),
        Col32_Bg(80), hint);

    if (bodyFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
