#include "UI_Loadout.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cstdio>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// 装备数据（硬编码，后续对接道具系统替换）
// ============================================================

struct EquipEntry {
    const char* name;
    const char* desc;
    int8_t      maxCarry;
    uint8_t     type;       // 1=item, 2=weapon
};

static constexpr EquipEntry kItems[] = {
    {"Cardboard Box",  "Invisible when stationary",     1, 1},
    {"Honeypot",       "Lure enemies on throw",         2, 1},
    {"Photon Radar",   "Reveal map & enemies",          1, 1},
    {"Cat Meme",       "6s full stealth",               1, 1},
    {"Phishing Link",  "Blind enemies 5s",              1, 1},
    {"Rock",           "Distract on throw",             5, 1},
};
static constexpr int kItemCount = 6;

static constexpr EquipEntry kWeapons[] = {
    {"Null Pointer",   "Eliminate target",              3, 2},
    {"DDOS Attack",    "Blind enemies in range",        2, 2},
    {"Cyber Mine",     "Extend remaining time",         1, 2},
};
static constexpr int kWeaponCount = 3;

static constexpr int kTotalCount = kItemCount + kWeaponCount;  // 9

// ============================================================
// 辅助：获取装备条目（统一索引 0..8）
// ============================================================

static const EquipEntry& GetEntry(int index) {
    if (index < kItemCount) return kItems[index];
    return kWeapons[index - kItemCount];
}

// ============================================================
// RenderLoadoutScreen
// ============================================================

void RenderLoadoutScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##Loadout", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float vpW = vp->Size.x;
    const float vpH = vp->Size.y;

    // ── 面板尺寸与居中 ──
    constexpr float PANEL_W = 600.0f;
    constexpr float PANEL_H = 460.0f;
    const float panelX = vp->Pos.x + (vpW - PANEL_W) * 0.5f;
    const float panelY = vp->Pos.y + (vpH - PANEL_H) * 0.5f;

    // 面板背景 + 边框
    draw->AddRectFilled(ImVec2(panelX, panelY),
                        ImVec2(panelX + PANEL_W, panelY + PANEL_H),
                        IM_COL32(8, 14, 20, 240), 3.0f);
    draw->AddRect(ImVec2(panelX, panelY),
                  ImVec2(panelX + PANEL_W, panelY + PANEL_H),
                  IM_COL32(0, 140, 130, 100), 3.0f, 0, 1.0f);

    float curY = panelY + 16.0f;

    // ── 标题 ──
    {
        ImFont* titleFont = UITheme::GetFont_TerminalLarge();
        if (titleFont) ImGui::PushFont(titleFont);
        const char* title = "LOADOUT";
        ImVec2 titleSize = ImGui::CalcTextSize(title);
        float titleX = panelX + (PANEL_W - titleSize.x) * 0.5f;
        draw->AddText(ImVec2(titleX, curY), IM_COL32(0, 220, 210, 255), title);
        if (titleFont) ImGui::PopFont();
        curY += titleSize.y + 4.0f;
    }

    // ── 副标题 ──
    {
        ImFont* smallFont = UITheme::GetFont_Small();
        if (smallFont) ImGui::PushFont(smallFont);
        const char* sub = "Select equipment before deployment";
        ImVec2 subSize = ImGui::CalcTextSize(sub);
        float subX = panelX + (PANEL_W - subSize.x) * 0.5f;
        draw->AddText(ImVec2(subX, curY), IM_COL32(130, 140, 150, 180), sub);
        if (smallFont) ImGui::PopFont();
        curY += subSize.y + 10.0f;
    }

    // ── 分隔线 ──
    draw->AddLine(ImVec2(panelX + 16.0f, curY),
                  ImVec2(panelX + PANEL_W - 16.0f, curY),
                  IM_COL32(0, 140, 130, 80), 1.0f);
    curY += 12.0f;

    // ── 双栏布局参数 ──
    constexpr float COL_W    = 272.0f;
    constexpr float COL_GAP  = 24.0f;
    constexpr float CARD_H   = 48.0f;
    constexpr float CARD_GAP = 5.0f;
    constexpr float STRIPE_W = 4.0f;
    const float colLX = panelX + (PANEL_W - COL_W * 2 - COL_GAP) * 0.5f;
    const float colRX = colLX + COL_W + COL_GAP;

    // ── 键盘导航 ──
    const Keyboard* kb = Window::GetKeyboard();
    int8_t& sel = ui.loadoutSelectedIndex;
    if (sel < 0) sel = 0;
    if (sel >= kTotalCount) sel = kTotalCount - 1;

    if (kb) {
        // W/S：上下移动（栏内）
        if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
            if (sel < kItemCount) {
                sel = (sel - 1 + kItemCount) % kItemCount;
            } else {
                int wi = sel - kItemCount;
                wi = (wi - 1 + kWeaponCount) % kWeaponCount;
                sel = static_cast<int8_t>(wi + kItemCount);
            }
        }
        if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
            if (sel < kItemCount) {
                sel = (sel + 1) % kItemCount;
            } else {
                int wi = sel - kItemCount;
                wi = (wi + 1) % kWeaponCount;
                sel = static_cast<int8_t>(wi + kItemCount);
            }
        }
        // A/D：切换左右栏
        if (kb->KeyPressed(KeyCodes::A) || kb->KeyPressed(KeyCodes::LEFT)) {
            if (sel >= kItemCount) {
                int wi = sel - kItemCount;
                sel = (wi < kItemCount) ? static_cast<int8_t>(wi) : 0;
            }
        }
        if (kb->KeyPressed(KeyCodes::D) || kb->KeyPressed(KeyCodes::RIGHT)) {
            if (sel < kItemCount) {
                int wi = sel;
                if (wi >= kWeaponCount) wi = kWeaponCount - 1;
                sel = static_cast<int8_t>(wi + kItemCount);
            }
        }
    }

    // ── 栏标题函数 ──
    auto DrawColumnHeader = [&](float x, float y, const char* label) {
        ImFont* termFont = UITheme::GetFont_Terminal();
        if (termFont) ImGui::PushFont(termFont);
        draw->AddText(ImVec2(x + 8.0f, y), IM_COL32(0, 200, 190, 200), label);
        if (termFont) ImGui::PopFont();
    };

    // ── 左栏 ">> ITEMS" ──
    DrawColumnHeader(colLX, curY, ">> ITEMS");
    float itemStartY = curY + 22.0f;

    // ── 右栏 ">> WEAPONS" ──
    DrawColumnHeader(colRX, curY, ">> WEAPONS");
    float weapStartY = curY + 22.0f;

    // ── 绘制卡片的 lambda ──
    auto DrawCard = [&](int globalIdx, float cx, float cy) {
        const auto& e = GetEntry(globalIdx);
        bool isSelected = (sel == globalIdx);
        float offsetX = isSelected ? 4.0f : 0.0f;

        float x = cx + offsetX;
        float y = cy;

        // 卡片背景
        ImU32 bgColor = isSelected ? IM_COL32(15, 22, 32, 240) : IM_COL32(10, 14, 20, 200);
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + COL_W - offsetX, y + CARD_H),
                            bgColor, 3.0f);

        // 选中边框
        if (isSelected) {
            draw->AddRect(ImVec2(x, y), ImVec2(x + COL_W - offsetX, y + CARD_H),
                          IM_COL32(0, 220, 210, 200), 3.0f, 0, 1.0f);
        } else {
            draw->AddRect(ImVec2(x, y), ImVec2(x + COL_W - offsetX, y + CARD_H),
                          IM_COL32(0, 80, 75, 60), 3.0f, 0, 1.0f);
        }

        // 左侧类型色条
        ImU32 stripeColor = (e.type == 2) ? IM_COL32(220, 140, 0, 200) : IM_COL32(0, 200, 190, 200);
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + STRIPE_W, y + CARD_H),
                            stripeColor, 3.0f, ImDrawFlags_RoundCornersLeft);

        // 裁剪到卡片范围
        draw->PushClipRect(ImVec2(x, y), ImVec2(x + COL_W - offsetX, y + CARD_H), true);

        // 名称
        ImFont* termFont = UITheme::GetFont_Terminal();
        if (termFont) ImGui::PushFont(termFont);
        ImU32 nameColor = isSelected ? IM_COL32(0, 220, 210, 255) : IM_COL32(200, 210, 215, 220);
        draw->AddText(ImVec2(x + STRIPE_W + 8.0f, y + 4.0f), nameColor, e.name);
        if (termFont) ImGui::PopFont();

        // 描述 + 携带上限
        ImFont* smallFont = UITheme::GetFont_Small();
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(ImVec2(x + STRIPE_W + 8.0f, y + 24.0f),
                      IM_COL32(130, 140, 150, 180), e.desc);

        char maxBuf[16];
        snprintf(maxBuf, sizeof(maxBuf), "Max: %d", e.maxCarry);
        ImVec2 maxSize = ImGui::CalcTextSize(maxBuf);
        draw->AddText(ImVec2(x + COL_W - offsetX - maxSize.x - 10.0f, y + 24.0f),
                      IM_COL32(0, 160, 150, 160), maxBuf);
        if (smallFont) ImGui::PopFont();

        draw->PopClipRect();

        // 鼠标 hover 检测
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x >= cx && mousePos.x <= cx + COL_W &&
            mousePos.y >= cy && mousePos.y <= cy + CARD_H) {
            sel = static_cast<int8_t>(globalIdx);
        }
    };

    // ── 绘制左栏卡片 ──
    for (int i = 0; i < kItemCount; ++i) {
        float cy = itemStartY + i * (CARD_H + CARD_GAP);
        DrawCard(i, colLX, cy);
    }

    // ── 绘制右栏卡片 ──
    for (int i = 0; i < kWeaponCount; ++i) {
        float cy = weapStartY + i * (CARD_H + CARD_GAP);
        DrawCard(i + kItemCount, colRX, cy);
    }

    // ── 底部分隔线 ──
    float bottomSepY = panelY + PANEL_H - 52.0f;
    draw->AddLine(ImVec2(panelX + 16.0f, bottomSepY),
                  ImVec2(panelX + PANEL_W - 16.0f, bottomSepY),
                  IM_COL32(0, 140, 130, 80), 1.0f);

    // ── BACK 按钮 ──
    {
        float btnW = 120.0f;
        float btnH = 32.0f;
        float btnX = panelX + (PANEL_W - btnW) * 0.5f;
        float btnY = bottomSepY + 10.0f;
        ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.08f, 0.12f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.18f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.60f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.85f, 0.80f, 1.0f));

        if (ImGui::Button("< BACK", ImVec2(btnW, btnH))) {
            ui.activeScreen = UIScreen::MainMenu;
            ui.previousScreen = UIScreen::Loadout;
        }

        ImGui::PopStyleColor(4);
    }

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
