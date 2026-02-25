#include "UI_ItemWheel.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// 轮盘扇区数量 = 道具2 + 武器2 = 4
static constexpr int kWheelSectors = 4;
static constexpr float kPI = 3.14159265f;

void RenderItemWheel(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    if (!registry.has_ctx<Res_GameplayState>()) return;

    auto& ui = registry.ctx<Res_UIState>();
    auto& gs = registry.ctx<Res_GameplayState>();

    const Keyboard* kb = Window::GetKeyboard();

    // Tab 按住打开轮盘
    if (kb) {
        if (kb->KeyDown(KeyCodes::TAB)) {
            if (!ui.itemWheelOpen) {
                ui.itemWheelOpen = true;
                ui.itemWheelSelected = -1;
            }
        } else if (ui.itemWheelOpen) {
            // Tab 释放 → 确认选择并关闭
            if (ui.itemWheelSelected >= 0) {
                if (ui.itemWheelSelected < 2) {
                    gs.activeItemSlot = static_cast<uint8_t>(ui.itemWheelSelected);
                    LOG_INFO("[UI_ItemWheel] Selected item slot " << (int)gs.activeItemSlot);
                } else {
                    gs.activeWeaponSlot = static_cast<uint8_t>(ui.itemWheelSelected - 2);
                    LOG_INFO("[UI_ItemWheel] Selected weapon slot " << (int)gs.activeWeaponSlot);
                }
            }
            ui.itemWheelOpen = false;
            return;
        }
    }

    if (!ui.itemWheelOpen) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    // 游戏区域宽度（排除聊天面板）
    float gameW = vpSize.x;
    if (registry.has_ctx<Res_ChatState>()) {
        gameW -= Res_ChatState::PANEL_WIDTH;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 轮盘居中在游戏区域
    float cx = vpPos.x + gameW * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.5f;
    float outerR = 120.0f;
    float innerR = 40.0f;

    // 半透明背景（仅覆盖游戏区域）
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + gameW, vpPos.y + vpSize.y),
        IM_COL32(0, 0, 0, 100));

    // 根据鼠标位置计算选中扇区
    ImVec2 mousePos = ImGui::GetMousePos();
    float dx = mousePos.x - cx;
    float dy = mousePos.y - cy;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist > innerR) {
        float angle = atan2f(dy, dx);
        if (angle < 0) angle += 2.0f * kPI;
        // 扇区: 0=右上(item0), 1=右下(item1), 2=左下(weapon0), 3=左上(weapon1)
        float sectorAngle = 2.0f * kPI / kWheelSectors;
        // 偏移使第一个扇区从正上方开始
        float adjusted = angle + sectorAngle * 0.5f;
        if (adjusted >= 2.0f * kPI) adjusted -= 2.0f * kPI;
        ui.itemWheelSelected = static_cast<int8_t>((int)(adjusted / sectorAngle) % kWheelSectors);
    } else {
        ui.itemWheelSelected = -1;
    }

    // 绘制扇区
    float sectorAngle = 2.0f * kPI / kWheelSectors;
    float startOffset = -kPI * 0.5f; // 从正上方开始

    const char* sectorLabels[4] = {};
    char labelBufs[4][24];

    // 填充标签
    for (int i = 0; i < 2; ++i) {
        if (gs.itemSlots[i].name[0]) {
            snprintf(labelBufs[i], sizeof(labelBufs[i]), "%s", gs.itemSlots[i].name);
        } else {
            snprintf(labelBufs[i], sizeof(labelBufs[i]), "Item %d", i + 1);
        }
        sectorLabels[i] = labelBufs[i];
    }
    for (int i = 0; i < 2; ++i) {
        if (gs.weaponSlots[i].name[0]) {
            snprintf(labelBufs[i + 2], sizeof(labelBufs[i + 2]), "%s", gs.weaponSlots[i].name);
        } else {
            snprintf(labelBufs[i + 2], sizeof(labelBufs[i + 2]), "Weapon %d", i + 1);
        }
        sectorLabels[i + 2] = labelBufs[i + 2];
    }

    for (int i = 0; i < kWheelSectors; ++i) {
        float a1 = startOffset + i * sectorAngle;
        float a2 = a1 + sectorAngle;
        bool isSelected = (i == ui.itemWheelSelected);

        // 扇区弧线
        ImU32 sectorColor = isSelected ? IM_COL32(0, 220, 210, 80)
                                       : IM_COL32(10, 14, 20, 200);
        ImU32 borderColor = isSelected ? IM_COL32(0, 220, 210, 200)
                                       : IM_COL32(0, 100, 95, 120);

        // 绘制扇区（使用多段弧近似）
        constexpr int arcSegs = 12;
        ImVec2 outerPts[arcSegs + 1];
        ImVec2 innerPts[arcSegs + 1];
        for (int s = 0; s <= arcSegs; ++s) {
            float t = a1 + (a2 - a1) * ((float)s / arcSegs);
            outerPts[s] = ImVec2(cx + cosf(t) * outerR, cy + sinf(t) * outerR);
            innerPts[s] = ImVec2(cx + cosf(t) * innerR, cy + sinf(t) * innerR);
        }

        // 填充扇区
        for (int s = 0; s < arcSegs; ++s) {
            draw->AddTriangleFilled(innerPts[s], outerPts[s], outerPts[s + 1], sectorColor);
            draw->AddTriangleFilled(innerPts[s], outerPts[s + 1], innerPts[s + 1], sectorColor);
        }

        // 扇区边界线
        draw->AddLine(
            ImVec2(cx + cosf(a1) * innerR, cy + sinf(a1) * innerR),
            ImVec2(cx + cosf(a1) * outerR, cy + sinf(a1) * outerR),
            borderColor, 1.0f);

        // 标签文字
        float midAngle = (a1 + a2) * 0.5f;
        float labelR = (innerR + outerR) * 0.5f;
        float lx = cx + cosf(midAngle) * labelR;
        float ly = cy + sinf(midAngle) * labelR;

        ImFont* smallFont = UITheme::GetFont_Small();
        if (smallFont) ImGui::PushFont(smallFont);

        ImVec2 textSize = ImGui::CalcTextSize(sectorLabels[i]);
        ImU32 textColor = isSelected ? IM_COL32(0, 220, 210, 255)
                                     : IM_COL32(130, 140, 150, 200);
        draw->AddText(ImVec2(lx - textSize.x * 0.5f, ly - textSize.y * 0.5f),
            textColor, sectorLabels[i]);

        if (smallFont) ImGui::PopFont();
    }

    // 外圈
    draw->AddCircle(ImVec2(cx, cy), outerR, IM_COL32(0, 140, 130, 100), 64, 1.0f);
    // 内圈
    draw->AddCircle(ImVec2(cx, cy), innerR, IM_COL32(0, 140, 130, 80), 32, 1.0f);

    // 中心文字
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* centerText = "SELECT";
    ImVec2 ctSize = ImGui::CalcTextSize(centerText);
    draw->AddText(ImVec2(cx - ctSize.x * 0.5f, cy - ctSize.y * 0.5f),
        IM_COL32(0, 180, 170, 180), centerText);

    if (termFont) ImGui::PopFont();

    // 底部提示
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(ImVec2(cx - 80.0f, cy + outerR + 15.0f),
        IM_COL32(60, 70, 75, 150),
        "[TAB] Hold to select, release to confirm");
    if (smallFont) ImGui::PopFont();
}

} // namespace ECS::UI

#endif // USE_IMGUI
