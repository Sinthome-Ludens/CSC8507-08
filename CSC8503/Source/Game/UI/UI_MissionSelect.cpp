/**
 * @file UI_MissionSelect.cpp
 * @brief 关卡选择界面实现：两列布局（道具/武器）、键鼠导航、装备选择、DEPLOY 触发。
 *
 * @details
 * 菜单阶段 Res_ItemInventory2 不存在时，使用临时默认实例并从存档缓存
 * （Res_UIState.savedStoreCount）恢复 storeCount 以显示上次库存。
 * DEPLOY 仅设置 pendingSceneRequest=StartGame，装备同步延迟到
 * Scene_PhysicsTest::OnEnter 执行（此时 Res_GameState 已创建）。
 */
#include "UI_MissionSelect.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Toast.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// RenderMissionSelect
// ============================================================

/**
 * @brief 渲染关卡选择界面（两列布局：道具 / 武器）并处理导航输入。
 * @param registry ECS 注册表（读写 Res_UIState 的 Tab/Cursor/EquippedItems/Weapons；
 *                 读 Res_ItemInventory2 库存数据，菜单阶段使用 fallback + savedStoreCount）
 * @details 键盘 A/D 切 Tab、W/S 导航、Enter 装备/选择、C 触发 DEPLOY（设置 pendingSceneRequest=StartGame）。
 *          鼠标悬浮自动高亮条目，左键点击可直接选择/装备。
 */
void RenderMissionSelect(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    ImGui::SetNextWindowPos(vpPos);
    ImGui::SetNextWindowSize(vpSize);
    ImGui::Begin("##MissionSelect", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Background #F5EEE8
    draw->AddRectFilled(vpPos,
        ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y),
        IM_COL32(245, 238, 232, 255));

    // ── Title ──────────────────────────────────────────────
    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(vpPos.x + 40.0f, vpPos.y + 30.0f),
        IM_COL32(16, 13, 10, 255), "MISSION SELECT");
    if (titleFont) ImGui::PopFont();

    float headerLineY = vpPos.y + 70.0f;
    draw->AddLine(
        ImVec2(vpPos.x + 40.0f, headerLineY),
        ImVec2(vpPos.x + vpSize.x - 40.0f, headerLineY),
        IM_COL32(200, 200, 200, 100), 1.0f);

    // ── Gather items/weapons from Res_ItemInventory2 ───────
    struct DisplaySlot {
        const char* name;
        const char* desc;
        uint8_t     carried;
        uint8_t     maxCarry;
        int         invIndex; // index into Res_ItemInventory2.slots[]
    };

    DisplaySlot gadgets[Res_ItemInventory2::kItemCount] = {};
    int gadgetCount = 0;
    DisplaySlot weapons[Res_ItemInventory2::kItemCount] = {};
    int weaponCount = 0;
    char descBuf[Res_ItemInventory2::kItemCount][80] = {};

    // ctx 中可能还没有 Res_ItemInventory2（主菜单阶段 Sys_Item 尚未注册），
    // 此时用临时默认实例读取道具列表，并从存档缓存恢复 storeCount。
    Res_ItemInventory2 fallbackInv;
    if (!registry.has_ctx<Res_ItemInventory2>() && ui.hasSavedInventory) {
        int limit = std::min(fallbackInv.kItemCount,
                             static_cast<int>(std::size(ui.savedStoreCount)));
        for (int i = 0; i < limit; ++i) {
            fallbackInv.slots[i].storeCount = ui.savedStoreCount[i];
            fallbackInv.slots[i].unlocked   = ui.savedUnlocked[i];
        }
    }
    Res_ItemInventory2& inv = registry.has_ctx<Res_ItemInventory2>()
                              ? registry.ctx<Res_ItemInventory2>()
                              : fallbackInv;

    for (int i = 0; i < inv.kItemCount; ++i) {
        auto& slot = inv.slots[i];
        DisplaySlot ds;
        ds.name     = slot.name;
        ds.carried  = slot.carriedCount;
        ds.maxCarry = slot.maxCarry;
        ds.invIndex = i;
        if (slot.itemType == ItemType::Gadget) {
            // Append stock info to description
            snprintf(descBuf[i], sizeof(descBuf[i]), "%s [Stock: %d]",
                     slot.desc, static_cast<int>(slot.storeCount));
            ds.desc = descBuf[i];
            if (gadgetCount < Res_ItemInventory2::kItemCount) gadgets[gadgetCount++] = ds;
        } else {
            // Only show unlocked weapons
            if (!slot.unlocked) continue;
            ds.desc = slot.desc;
            if (weaponCount < Res_ItemInventory2::kItemCount) weapons[weaponCount++] = ds;
        }
    }

    // ── Keyboard navigation ────────────────────────────────
    const Keyboard* kb = Window::GetKeyboard();
    constexpr int kTabCount = 2;
    int tabItemCounts[kTabCount] = { gadgetCount, weaponCount };

    if (kb) {
        // A/D: switch tab
        if (kb->KeyPressed(KeyCodes::A) || kb->KeyPressed(KeyCodes::LEFT)) {
            ui.missionSelectedTab = (ui.missionSelectedTab - 1 + kTabCount) % kTabCount;
        }
        if (kb->KeyPressed(KeyCodes::D) || kb->KeyPressed(KeyCodes::RIGHT)) {
            ui.missionSelectedTab = (ui.missionSelectedTab + 1) % kTabCount;
        }

        int curTab = ui.missionSelectedTab;
        int maxIdx = tabItemCounts[curTab];
        if (maxIdx > 0) {
            // W/S: navigate within tab
            if (kb->KeyPressed(KeyCodes::W) || kb->KeyPressed(KeyCodes::UP)) {
                ui.missionCursorPerTab[curTab] =
                    (ui.missionCursorPerTab[curTab] - 1 + maxIdx) % maxIdx;
            }
            if (kb->KeyPressed(KeyCodes::S) || kb->KeyPressed(KeyCodes::DOWN)) {
                ui.missionCursorPerTab[curTab] =
                    (ui.missionCursorPerTab[curTab] + 1) % maxIdx;
            }
        }
    }

    // ── Layout: two columns ───────────────────────────────
    float padX   = 40.0f;
    float gapX   = 20.0f;
    float usableW = vpSize.x - padX * 2 - gapX * 1;
    float colW   = usableW / 2.0f;
    float col0X  = vpPos.x + padX;
    float col1X  = col0X + colW + gapX;
    float startY = headerLineY + 15.0f;
    float entryH = 50.0f;

    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();
    const Mouse* mouse = Window::GetMouse();

    // ── Tab headers ────────────────────────────────────────
    const char* tabLabels[] = { "ITEMS (MAX 2)", "WEAPONS (MAX 2)" };
    float tabXs[] = { col0X, col1X };

    if (termFont) ImGui::PushFont(termFont);
    for (int t = 0; t < kTabCount; ++t) {
        bool isActiveTab = (t == ui.missionSelectedTab);
        ImU32 tabColor = isActiveTab ? IM_COL32(252, 111, 41, 255)
                                     : IM_COL32(16, 13, 10, 160);
        draw->AddText(ImVec2(tabXs[t], startY), tabColor, tabLabels[t]);

        if (isActiveTab) {
            ImVec2 labelSize = ImGui::CalcTextSize(tabLabels[t]);
            draw->AddLine(
                ImVec2(tabXs[t], startY + labelSize.y + 2.0f),
                ImVec2(tabXs[t] + labelSize.x, startY + labelSize.y + 2.0f),
                IM_COL32(252, 111, 41, 200), 2.0f);
        }
    }
    if (termFont) ImGui::PopFont();

    float entryStartY = startY + 35.0f;

    // ── Equip helpers ──────────────────────────────────────
    auto isItemEquipped = [&](int idx) -> bool {
        return ui.missionEquippedItems[0] == idx || ui.missionEquippedItems[1] == idx;
    };
    auto isWeaponEquipped = [&](int idx) -> bool {
        return ui.missionEquippedWeapons[0] == idx || ui.missionEquippedWeapons[1] == idx;
    };
    auto countEquippedItems = [&]() -> int {
        int c = 0;
        if (ui.missionEquippedItems[0] >= 0) c++;
        if (ui.missionEquippedItems[1] >= 0) c++;
        return c;
    };
    auto countEquippedWeapons = [&]() -> int {
        int c = 0;
        if (ui.missionEquippedWeapons[0] >= 0) c++;
        if (ui.missionEquippedWeapons[1] >= 0) c++;
        return c;
    };

    // ── Column divider ─────────────────────────────────────
    float divH = entryStartY + 5 * entryH;
    draw->AddLine(ImVec2(col1X - gapX * 0.5f, startY),
                  ImVec2(col1X - gapX * 0.5f, divH),
                  IM_COL32(200, 200, 200, 80), 1.0f);

    // ── Draw column entries ────────────────────────────────
    // Generic entry drawing lambda
    auto drawEntries = [&](int tabIdx, float colX, int count,
                           const char* const* names, const char* const* descs,
                           bool isGadgetCol, bool isWeaponCol,
                           const DisplaySlot* slots) {
        bool isActiveTab = (tabIdx == ui.missionSelectedTab);
        for (int i = 0; i < count; ++i) {
            float itemY = entryStartY + i * entryH;
            bool isCursor = isActiveTab && (i == ui.missionCursorPerTab[tabIdx]);

            bool equipped = false;
            if (isGadgetCol)  equipped = isItemEquipped(i);
            if (isWeaponCol)  equipped = isWeaponEquipped(i);

            ImVec2 itemMin(colX - 5.0f, itemY - 4.0f);
            ImVec2 itemMax(colX + colW, itemY + entryH - 8.0f);

            // Mouse hover
            if (mouse) {
                ImVec2 mp = ImGui::GetMousePos();
                if (mp.x >= itemMin.x && mp.x <= itemMax.x &&
                    mp.y >= itemMin.y && mp.y <= itemMax.y) {
                    if (isActiveTab) {
                        ui.missionCursorPerTab[tabIdx] = static_cast<int8_t>(i);
                        isCursor = true;
                    }
                }
            }

            // Highlight
            if (equipped) {
                draw->AddRectFilled(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 40), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 180), 2.0f, 0, 1.5f);
            } else if (isCursor) {
                draw->AddRectFilled(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 25), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    IM_COL32(252, 111, 41, 120), 2.0f, 0, 1.0f);
            }

            // Name
            if (termFont) ImGui::PushFont(termFont);
            const char* displayName = names ? names[i] : (slots ? slots[i].name : "");
            char nameBuf[64];
            if (equipped) {
                snprintf(nameBuf, sizeof(nameBuf), "> %s [EQUIPPED]", displayName);
            } else {
                snprintf(nameBuf, sizeof(nameBuf), isCursor ? "> %s" : "  %s", displayName);
            }
            ImU32 nameColor = equipped ? IM_COL32(252, 111, 41, 255)
                            : isCursor ? IM_COL32(16, 13, 10, 255)
                            : IM_COL32(16, 13, 10, 220);
            draw->AddText(ImVec2(colX + (isCursor || equipped ? 4.0f : 0.0f), itemY),
                nameColor, nameBuf);
            if (termFont) ImGui::PopFont();

            // Description
            if (smallFont) ImGui::PushFont(smallFont);
            const char* descText = descs ? descs[i] : (slots ? slots[i].desc : "");
            draw->AddText(ImVec2(colX + 18.0f, itemY + 20.0f),
                IM_COL32(16, 13, 10, 150), descText);
            if (smallFont) ImGui::PopFont();
        }
    };

    // Col 0: Gadgets
    {
        const char* gNames[Res_ItemInventory2::kItemCount] = {};
        const char* gDescs[Res_ItemInventory2::kItemCount] = {};
        for (int i = 0; i < gadgetCount; ++i) {
            gNames[i] = gadgets[i].name;
            gDescs[i] = gadgets[i].desc;
        }
        drawEntries(0, col0X, gadgetCount, gNames, gDescs, true, false, gadgets);
    }

    // Col 1: Weapons
    if (weaponCount > 0) {
        const char* wNames[Res_ItemInventory2::kItemCount] = {};
        const char* wDescs[Res_ItemInventory2::kItemCount] = {};
        for (int i = 0; i < weaponCount; ++i) {
            wNames[i] = weapons[i].name;
            wDescs[i] = weapons[i].desc;
        }
        drawEntries(1, col1X, weaponCount, wNames, wDescs, false, true, weapons);
    } else {
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(ImVec2(col1X, entryStartY + 10.0f),
            IM_COL32(16, 13, 10, 120),
            "FIND WEAPONS ON THE MAP TO UNLOCK");
        if (smallFont) ImGui::PopFont();
    }

    // ── Enter key: equip/unequip or select map ─────────────
    bool enterPressed = kb && (kb->KeyPressed(KeyCodes::RETURN) || kb->KeyPressed(KeyCodes::SPACE));
    bool mouseClicked = mouse && mouse->ButtonPressed(NCL::MouseButtons::Left);

    // Determine clicked tab/index from mouse
    int clickedTab = -1;
    int clickedIdx = -1;
    if (mouseClicked) {
        ImVec2 mp = ImGui::GetMousePos();
        float tabColXs[] = { col0X, col1X };
        int   tabCounts[] = { gadgetCount, weaponCount };
        for (int t = 0; t < kTabCount; ++t) {
            for (int i = 0; i < tabCounts[t]; ++i) {
                float itemY = entryStartY + i * entryH;
                ImVec2 itemMin(tabColXs[t] - 5.0f, itemY - 4.0f);
                ImVec2 itemMax(tabColXs[t] + colW, itemY + entryH - 8.0f);
                if (mp.x >= itemMin.x && mp.x <= itemMax.x &&
                    mp.y >= itemMin.y && mp.y <= itemMax.y) {
                    clickedTab = t;
                    clickedIdx = i;
                    break;
                }
            }
            if (clickedTab >= 0) break;
        }
    }

    // Process selection
    auto processSelect = [&](int tab, int idx) {
        if (tab == 0) {
            // Gadget equip/unequip
            if (isItemEquipped(idx)) {
                if (ui.missionEquippedItems[0] == idx) ui.missionEquippedItems[0] = -1;
                else if (ui.missionEquippedItems[1] == idx) ui.missionEquippedItems[1] = -1;
            } else if (countEquippedItems() < 2) {
                if (ui.missionEquippedItems[0] < 0) ui.missionEquippedItems[0] = static_cast<int8_t>(idx);
                else ui.missionEquippedItems[1] = static_cast<int8_t>(idx);
            } else {
                PushToast(registry, "MAX 2 ITEMS", ToastType::Warning);
            }
        } else if (tab == 1) {
            // Weapon equip/unequip
            if (isWeaponEquipped(idx)) {
                if (ui.missionEquippedWeapons[0] == idx) ui.missionEquippedWeapons[0] = -1;
                else if (ui.missionEquippedWeapons[1] == idx) ui.missionEquippedWeapons[1] = -1;
            } else if (countEquippedWeapons() < 2) {
                if (ui.missionEquippedWeapons[0] < 0) ui.missionEquippedWeapons[0] = static_cast<int8_t>(idx);
                else ui.missionEquippedWeapons[1] = static_cast<int8_t>(idx);
            } else {
                PushToast(registry, "MAX 2 WEAPONS", ToastType::Warning);
            }
        }
    };

    if (enterPressed) {
        int curTab = ui.missionSelectedTab;
        int curIdx = ui.missionCursorPerTab[curTab];
        if (curIdx >= 0 && curIdx < tabItemCounts[curTab]) {
            processSelect(curTab, curIdx);
        }
    }
    if (clickedTab >= 0 && clickedIdx >= 0) {
        processSelect(clickedTab, clickedIdx);
    }

    // ── DEPLOY button ──────────────────────────────────────
    float btnW = 240.0f;
    float btnH = 36.0f;
    float btnX = vpPos.x + (vpSize.x - btnW) * 0.5f;
    float btnY = vpPos.y + vpSize.y - 80.0f;
    ImVec2 btnMin(btnX, btnY);
    ImVec2 btnMax(btnX + btnW, btnY + btnH);

    bool btnHovered = false;
    if (mouse) {
        ImVec2 mp = ImGui::GetMousePos();
        btnHovered = (mp.x >= btnMin.x && mp.x <= btnMax.x &&
                      mp.y >= btnMin.y && mp.y <= btnMax.y);
    }

    draw->AddRectFilled(btnMin, btnMax,
        btnHovered ? IM_COL32(252, 111, 41, 200) : IM_COL32(252, 111, 41, 160), 3.0f);
    draw->AddRect(btnMin, btnMax,
        IM_COL32(252, 111, 41, 255), 3.0f);

    if (termFont) ImGui::PushFont(termFont);
    const char* btnText = "DEPLOY";
    ImVec2 btnTextSize = ImGui::CalcTextSize(btnText);
    draw->AddText(
        ImVec2(btnX + (btnW - btnTextSize.x) * 0.5f, btnY + (btnH - btnTextSize.y) * 0.5f),
        IM_COL32(245, 238, 232, 255), btnText);
    if (termFont) ImGui::PopFont();

    bool btnClicked = false;
    if (mouse && mouse->ButtonPressed(NCL::MouseButtons::Left) && btnHovered) {
        btnClicked = true;
    }
    if (kb && kb->KeyPressed(KeyCodes::C)) {
        btnClicked = true;
    }

    if (btnClicked) {
        // 装备同步在 Scene_PhysicsTest::OnEnter 中执行（菜单阶段无 Res_GameState）
        ui.pendingSceneRequest = SceneRequest::StartGame;
        LOG_INFO("[UI_MissionSelect] DEPLOY -> StartGame");
    }

    // ── Bottom hint ────────────────────────────────────────
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(vpPos.x + 40.0f, vpPos.y + vpSize.y - 30.0f),
        IM_COL32(16, 13, 10, 180),
        "[A/D] TAB  [W/S] SELECT  [ENTER] EQUIP  [C] DEPLOY  [ESC] BACK");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
