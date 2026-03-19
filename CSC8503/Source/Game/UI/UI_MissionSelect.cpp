/**
 * @file UI_MissionSelect.cpp
 * @brief 关卡选择界面实现：三列布局（关卡/道具/武器）、键鼠导航、装备选择、DEPLOY 触发。
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
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_UIKeyConfig.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Anim.h"
#include "Game/UI/UI_Toast.h"
#include "Game/Utils/Log.h"

using namespace NCL;
using namespace ECS::UITheme;

namespace ECS::UI {

// ============================================================
// 关卡描述（顺序匹配 Res_UIState.h 的 kMapDisplayNames: 0=HangerA,1=HangerB,2=Helipad,3=Lab,4=Dock）
// ============================================================
static const char* kMapDescs[] = {
    "Hanger A",
    "Hanger B",
    "Helipad",
    "Underground Lab",
    "Dock Area",
};

// ============================================================
// RenderMissionSelect
// ============================================================

/**
 * @brief 渲染关卡选择界面（三列布局：关卡 / 道具 / 武器）并处理导航输入。
 * @param registry ECS 注册表（读写 Res_UIState 的 missionSelectedMap/Tab/Cursor/EquippedItems/Weapons；
 *                 读 Res_ItemInventory2 库存数据，菜单阶段使用 fallback + savedStoreCount）
 * @details 键盘 A/D 切 Tab、W/S 导航、Enter 装备/选择、C 触发 DEPLOY（设置 pendingSceneRequest=StartGame）。
 *          鼠标悬浮自动高亮条目，左键点击可直接选择/装备。
 */
void RenderMissionSelect(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();
    const auto& input = registry.ctx<Res_Input>();
    const auto& uiCfg = registry.ctx<Res_UIKeyConfig>();

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
        Col32_Bg());

    // Entry animation + slide
    float entryRaw = (ui.screenEntryDuration > 0.0f)
        ? std::clamp(ui.screenEntryElapsed / ui.screenEntryDuration, 0.0f, 1.0f) : 1.0f;
    float entryT = Anim::EaseOutCubic(entryRaw);
    float slideX = Anim::SlideOffset(entryT, ui.transDirection);

    // ── Title ──────────────────────────────────────────────
    ImFont* titleFont = GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);
    draw->AddText(ImVec2(vpPos.x + 40.0f + slideX, vpPos.y + 30.0f),
        Col32_Text(), "MISSION SELECT");
    if (titleFont) ImGui::PopFont();

    float headerLineY = vpPos.y + 70.0f;
    draw->AddLine(
        ImVec2(vpPos.x + 40.0f + slideX, headerLineY),
        ImVec2(vpPos.x + vpSize.x - 40.0f + slideX, headerLineY),
        Col32_Gray(100), 1.0f);

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
    constexpr int kTabCount = 3;
    int tabItemCounts[kTabCount] = { kMapCount, gadgetCount, weaponCount };

    {
        // A/D: switch tab
        if (input.keyPressed[uiCfg.keyMenuLeft] || input.keyPressed[uiCfg.keyMenuLeftAlt]) {
            ui.missionSelectedTab = (ui.missionSelectedTab - 1 + kTabCount) % kTabCount;
        }
        if (input.keyPressed[uiCfg.keyMenuRight] || input.keyPressed[uiCfg.keyMenuRightAlt]) {
            ui.missionSelectedTab = (ui.missionSelectedTab + 1) % kTabCount;
        }

        int curTab = ui.missionSelectedTab;
        int maxIdx = tabItemCounts[curTab];
        if (maxIdx > 0) {
            // W/S: navigate within tab
            if (input.keyPressed[uiCfg.keyMenuUp] || input.keyPressed[uiCfg.keyMenuUpAlt]) {
                ui.missionCursorPerTab[curTab] =
                    (ui.missionCursorPerTab[curTab] - 1 + maxIdx) % maxIdx;
            }
            if (input.keyPressed[uiCfg.keyMenuDown] || input.keyPressed[uiCfg.keyMenuDownAlt]) {
                ui.missionCursorPerTab[curTab] =
                    (ui.missionCursorPerTab[curTab] + 1) % maxIdx;
            }
        }
    }

    // ── Layout: three columns ──────────────────────────────
    float padX   = 40.0f;
    float gapX   = 20.0f;
    float usableW = vpSize.x - padX * 2 - gapX * 2;
    float colW   = usableW / 3.0f;
    float col0X  = vpPos.x + padX;
    float col1X  = col0X + colW + gapX;
    float col2X  = col1X + colW + gapX;
    float startY = headerLineY + 15.0f;
    float entryH = 50.0f;

    ImFont* termFont  = GetFont_Terminal();
    ImFont* smallFont = GetFont_Small();
    // ── Tab headers ────────────────────────────────────────
    const char* tabLabels[] = { "MAP", "ITEMS (MAX 2)", "WEAPONS (MAX 2)" };
    float tabXs[] = { col0X, col1X, col2X };

    if (termFont) ImGui::PushFont(termFont);
    for (int t = 0; t < kTabCount; ++t) {
        bool isActiveTab = (t == ui.missionSelectedTab);
        ImU32 tabColor = isActiveTab ? Col32_Accent()
                                     : Col32_Text(160);
        draw->AddText(ImVec2(tabXs[t], startY), tabColor, tabLabels[t]);

        if (isActiveTab) {
            ImVec2 labelSize = ImGui::CalcTextSize(tabLabels[t]);
            draw->AddLine(
                ImVec2(tabXs[t], startY + labelSize.y + 2.0f),
                ImVec2(tabXs[t] + labelSize.x, startY + labelSize.y + 2.0f),
                Col32_Accent(200), 2.0f);
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

    // ── Column dividers ────────────────────────────────────
    float divH = entryStartY + 5 * entryH;
    draw->AddLine(ImVec2(col1X - gapX * 0.5f, startY),
                  ImVec2(col1X - gapX * 0.5f, divH),
                  Col32_Gray(80), 1.0f);
    draw->AddLine(ImVec2(col2X - gapX * 0.5f, startY),
                  ImVec2(col2X - gapX * 0.5f, divH),
                  Col32_Gray(80), 1.0f);

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
            if (tabIdx == 0)  equipped = (i == ui.missionSelectedMap);

            ImVec2 itemMin(colX - 5.0f, itemY - 4.0f);
            ImVec2 itemMax(colX + colW, itemY + entryH - 8.0f);

            // Mouse hover
            {
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
                    Col32_Accent(40), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    Col32_Accent(180), 2.0f, 0, 1.5f);
            } else if (isCursor) {
                draw->AddRectFilled(itemMin, itemMax,
                    Col32_Accent(25), 2.0f);
                draw->AddRect(itemMin, itemMax,
                    Col32_Accent(120), 2.0f, 0, 1.0f);
            }

            // Name
            if (termFont) ImGui::PushFont(termFont);
            const char* displayName = names ? names[i] : (slots ? slots[i].name : "");
            char nameBuf[64];
            if (equipped && tabIdx != 0) {
                snprintf(nameBuf, sizeof(nameBuf), "> %s [EQUIPPED]", displayName);
            } else if (equipped && tabIdx == 0) {
                snprintf(nameBuf, sizeof(nameBuf), "> %s [SELECTED]", displayName);
            } else {
                snprintf(nameBuf, sizeof(nameBuf), isCursor ? "> %s" : "  %s", displayName);
            }
            ImU32 nameColor = equipped ? Col32_Accent()
                            : isCursor ? Col32_Text()
                            : Col32_Text(220);
            draw->AddText(ImVec2(colX + (isCursor || equipped ? 4.0f : 0.0f), itemY),
                nameColor, nameBuf);
            if (termFont) ImGui::PopFont();

            // Description
            if (smallFont) ImGui::PushFont(smallFont);
            const char* descText = descs ? descs[i] : (slots ? slots[i].desc : "");
            draw->AddText(ImVec2(colX + 18.0f, itemY + 20.0f),
                Col32_Text(150), descText);
            if (smallFont) ImGui::PopFont();
        }
    };

    // Col 0: Maps (use kMapDisplayNames from Res_UIState.h)
    drawEntries(0, col0X, kMapCount, kMapDisplayNames, kMapDescs, false, false, nullptr);

    // Col 1: Gadgets
    {
        const char* gNames[Res_ItemInventory2::kItemCount] = {};
        const char* gDescs[Res_ItemInventory2::kItemCount] = {};
        for (int i = 0; i < gadgetCount; ++i) {
            gNames[i] = gadgets[i].name;
            gDescs[i] = gadgets[i].desc;
        }
        drawEntries(1, col1X, gadgetCount, gNames, gDescs, true, false, gadgets);
    }

    // Col 2: Weapons
    if (weaponCount > 0) {
        const char* wNames[Res_ItemInventory2::kItemCount] = {};
        const char* wDescs[Res_ItemInventory2::kItemCount] = {};
        for (int i = 0; i < weaponCount; ++i) {
            wNames[i] = weapons[i].name;
            wDescs[i] = weapons[i].desc;
        }
        drawEntries(2, col2X, weaponCount, wNames, wDescs, false, true, weapons);
    } else {
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(ImVec2(col2X, entryStartY + 10.0f),
            Col32_Text(120),
            "FIND WEAPONS ON THE MAP TO UNLOCK");
        if (smallFont) ImGui::PopFont();
    }

    // ── Enter key: equip/unequip or select map ─────────────
    bool enterPressed = input.keyPressed[uiCfg.keyConfirm] || input.keyPressed[uiCfg.keyConfirmAlt];
    bool mouseClicked = input.mouseButtonPressed[uiCfg.mouseConfirm];

    // Determine clicked tab/index from mouse
    int clickedTab = -1;
    int clickedIdx = -1;
    if (mouseClicked) {
        ImVec2 mp = ImGui::GetMousePos();
        float tabColXs[] = { col0X, col1X, col2X };
        int   tabCounts[] = { kMapCount, gadgetCount, weaponCount };
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
            // Map select
            ui.missionSelectedMap = static_cast<int8_t>(idx);
        } else if (tab == 1) {
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
        } else if (tab == 2) {
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
    {
        ImVec2 mp = ImGui::GetMousePos();
        btnHovered = (mp.x >= btnMin.x && mp.x <= btnMax.x &&
                      mp.y >= btnMin.y && mp.y <= btnMax.y);
    }

    draw->AddRectFilled(btnMin, btnMax,
        btnHovered ? Col32_Accent(200) : Col32_Accent(160), 3.0f);
    draw->AddRect(btnMin, btnMax,
        Col32_Accent(), 3.0f);

    if (termFont) ImGui::PushFont(termFont);
    const char* btnText = "DEPLOY";
    ImVec2 btnTextSize = ImGui::CalcTextSize(btnText);
    draw->AddText(
        ImVec2(btnX + (btnW - btnTextSize.x) * 0.5f, btnY + (btnH - btnTextSize.y) * 0.5f),
        Col32_Bg(), btnText);
    if (termFont) ImGui::PopFont();

    bool btnClicked = false;
    if (input.mouseButtonPressed[uiCfg.mouseConfirm] && btnHovered) {
        btnClicked = true;
    }
    if (input.keyPressed[uiCfg.keyDeploy]) {
        btnClicked = true;
    }

    if (btnClicked) {
        // 装备同步在 Scene_PhysicsTest::OnEnter 中执行（菜单阶段无 Res_GameState）
        ui.pendingSceneRequest = SceneRequest::StartGame;
        LOG_INFO("[UI_MissionSelect] DEPLOY -> StartGame (map="
                 << (int)ui.missionSelectedMap << ")");
    }

    // ── Bottom hint ────────────────────────────────────────
    if (smallFont) ImGui::PushFont(smallFont);
    draw->AddText(
        ImVec2(vpPos.x + 40.0f, vpPos.y + vpSize.y - 30.0f),
        Col32_Text(180),
        "[A/D] TAB  [W/S] SELECT  [ENTER] EQUIP  [C] DEPLOY  [ESC] BACK");
    if (smallFont) ImGui::PopFont();

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
