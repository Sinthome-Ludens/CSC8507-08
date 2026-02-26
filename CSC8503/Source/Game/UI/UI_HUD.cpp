#include "UI_HUD.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameplayState.h"
#include "Game/UI/UITheme.h"
#include "Game/UI/UI_Chat.h"
#include "Game/UI/UI_ItemWheel.h"
#include "Game/Components/Res_ChatState.h"

namespace ECS::UI {

// ============================================================
// HUD 子函数（file-local，仅在此 TU 中使用）
// ============================================================

static void RenderHUD_AlertGauge(ImDrawList* draw, float x, float y, float w, float h,
                                  float alertLevel, float alertMax) {
    AlertStatus status = GetAlertStatus(alertLevel);
    float ratio = (alertMax > 0.0f) ? alertLevel / alertMax : 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    // 背景面板
    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                        IM_COL32(8, 10, 16, 200), 3.0f);
    draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                  IM_COL32(0, 100, 95, 120), 3.0f, 0, 1.0f);

    // 标题
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(ImVec2(x + 8.0f, y + 4.0f),
                  IM_COL32(0, 140, 130, 180), "ALERT LEVEL");

    if (smallFont) ImGui::PopFont();

    // 进度条
    float barX = x + 8.0f;
    float barY = y + 20.0f;
    float barW = w - 16.0f;
    float barH = 10.0f;

    draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                        IM_COL32(20, 25, 30, 200), 2.0f);

    ImU32 barColor;
    switch (status) {
        case AlertStatus::Safe:   barColor = IM_COL32(0, 200, 190, 220);  break;
        case AlertStatus::Search: barColor = IM_COL32(200, 200, 0, 220);  break;
        case AlertStatus::Alert:  barColor = IM_COL32(220, 140, 0, 220);  break;
        case AlertStatus::Hunt:   barColor = IM_COL32(220, 60, 20, 220);  break;
        case AlertStatus::Raid:   barColor = IM_COL32(255, 20, 20, 255);  break;
        default:                  barColor = IM_COL32(0, 200, 190, 220);  break;
    }

    float fillW = barW * ratio;
    if (fillW > 0.0f) {
        draw->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH),
                            barColor, 2.0f);
    }

    // 状态文字 + 数值
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    char statusBuf[48];
    snprintf(statusBuf, sizeof(statusBuf), "%s  %.0f / %.0f",
             GetAlertStatusText(status), alertLevel, alertMax);
    draw->AddText(ImVec2(barX, barY + barH + 3.0f), barColor, statusBuf);

    if (termFont) ImGui::PopFont();
}

static void RenderHUD_Countdown(ImDrawList* draw, float cx, float y,
                                 float timer, bool active) {
    if (!active) return;

    ImFont* titleFont = UITheme::GetFont_TerminalLarge();
    if (titleFont) ImGui::PushFont(titleFont);

    int minutes = (int)(timer / 60.0f);
    int seconds = (int)(timer) % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);

    ImVec2 textSize = ImGui::CalcTextSize(buf);
    float textX = cx - textSize.x * 0.5f;

    // 发光底层
    draw->AddText(ImVec2(textX, y), IM_COL32(200, 20, 20, 60), buf);
    draw->AddText(ImVec2(textX - 1, y), IM_COL32(220, 40, 30, 120), buf);
    // 主层
    ImU32 timerColor = (timer < 30.0f) ? IM_COL32(255, 40, 30, 255)
                                       : IM_COL32(220, 180, 0, 255);
    draw->AddText(ImVec2(textX, y), timerColor, buf);

    if (titleFont) ImGui::PopFont();

    // 三角箭头装饰
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(ImVec2(textX - 25.0f, y + 5.0f),
                  IM_COL32(200, 60, 40, 150), ">>");
    draw->AddText(ImVec2(textX + textSize.x + 8.0f, y + 5.0f),
                  IM_COL32(200, 60, 40, 150), "<<");

    if (smallFont) ImGui::PopFont();
}

static void RenderHUD_MissionPanel(ImDrawList* draw, float x, float y,
                                    const char* missionName, const char* objective) {
    float panelW = 300.0f;
    float panelH = 52.0f;

    draw->AddRectFilled(ImVec2(x, y), ImVec2(x + panelW, y + panelH),
                        IM_COL32(8, 10, 16, 180), 3.0f);
    draw->AddRect(ImVec2(x, y), ImVec2(x + panelW, y + panelH),
                  IM_COL32(0, 100, 95, 80), 3.0f, 0, 1.0f);

    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    draw->AddText(ImVec2(x + 8.0f, y + 4.0f),
                  IM_COL32(0, 140, 130, 180),
                  (missionName && missionName[0]) ? missionName : "OPERATION");

    if (smallFont) ImGui::PopFont();

    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    char objBuf[64];
    snprintf(objBuf, sizeof(objBuf), "> %s",
             (objective && objective[0]) ? objective : "---");
    draw->AddText(ImVec2(x + 8.0f, y + 22.0f),
                  IM_COL32(0, 220, 210, 220), objBuf);

    if (termFont) ImGui::PopFont();
}

static void RenderHUD_PlayerState(ImDrawList* draw, float x, float y,
                                   uint8_t moveState, bool disguised) {
    ImFont* termFont = UITheme::GetFont_Terminal();
    if (termFont) ImGui::PushFont(termFont);

    const char* stateText;
    ImU32 stateColor;
    switch (static_cast<PlayerMoveState>(moveState)) {
        case PlayerMoveState::Crouching:
            stateText = "[CROUCH]";
            stateColor = IM_COL32(0, 180, 170, 200);
            break;
        case PlayerMoveState::Running:
            stateText = "[SPRINT]";
            stateColor = IM_COL32(220, 140, 0, 220);
            break;
        default:
            stateText = "[STAND]";
            stateColor = IM_COL32(100, 110, 120, 180);
            break;
    }
    draw->AddText(ImVec2(x, y), stateColor, stateText);

    if (disguised) {
        ImVec2 stSize = ImGui::CalcTextSize(stateText);
        draw->AddText(ImVec2(x + stSize.x + 12.0f, y),
                      IM_COL32(100, 200, 100, 220), "[DISGUISED]");
    }

    if (termFont) ImGui::PopFont();
}

static void RenderHUD_ItemSlots(ImDrawList* draw, float x, float y,
                                 const Res_GameplayState& gs) {
    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    float slotW = 120.0f;
    float slotH = 24.0f;
    float gap   = 8.0f;

    draw->AddText(ImVec2(x, y - 16.0f), IM_COL32(0, 140, 130, 150), "ITEMS");
    draw->AddText(ImVec2(x + slotW + gap, y - 16.0f), IM_COL32(0, 140, 130, 150), "WEAPONS");

    for (int i = 0; i < 2; ++i) {
        // 道具槽
        float sx = x;
        float sy = y + i * (slotH + 4.0f);
        bool isActiveItem = (i == gs.activeItemSlot);

        ImU32 borderColor = isActiveItem ? IM_COL32(0, 220, 210, 200)
                                         : IM_COL32(0, 80, 75, 100);
        draw->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + slotW, sy + slotH),
                            IM_COL32(8, 10, 16, 180), 2.0f);
        draw->AddRect(ImVec2(sx, sy), ImVec2(sx + slotW, sy + slotH),
                      borderColor, 2.0f, 0, 1.0f);

        char itemBuf[32];
        if (gs.itemSlots[i].name[0]) {
            snprintf(itemBuf, sizeof(itemBuf), "%s x%d",
                     gs.itemSlots[i].name, gs.itemSlots[i].count);
        } else {
            snprintf(itemBuf, sizeof(itemBuf), "---");
        }
        ImU32 textColor = isActiveItem ? IM_COL32(0, 220, 210, 220)
                                       : IM_COL32(80, 90, 100, 180);
        draw->AddText(ImVec2(sx + 6.0f, sy + 4.0f), textColor, itemBuf);

        // 冷却条
        if (gs.itemSlots[i].cooldown > 0.0f) {
            float cdW = slotW * gs.itemSlots[i].cooldown;
            draw->AddRectFilled(ImVec2(sx, sy + slotH - 3.0f),
                                ImVec2(sx + cdW, sy + slotH),
                                IM_COL32(220, 60, 20, 150));
        }

        // 武器槽
        float wx = x + slotW + gap;
        bool isActiveWeapon = (i == gs.activeWeaponSlot);

        ImU32 wBorderColor = isActiveWeapon ? IM_COL32(0, 220, 210, 200)
                                            : IM_COL32(0, 80, 75, 100);
        draw->AddRectFilled(ImVec2(wx, sy), ImVec2(wx + slotW, sy + slotH),
                            IM_COL32(8, 10, 16, 180), 2.0f);
        draw->AddRect(ImVec2(wx, sy), ImVec2(wx + slotW, sy + slotH),
                      wBorderColor, 2.0f, 0, 1.0f);

        char wpnBuf[32];
        if (gs.weaponSlots[i].name[0]) {
            snprintf(wpnBuf, sizeof(wpnBuf), "%s x%d",
                     gs.weaponSlots[i].name, gs.weaponSlots[i].count);
        } else {
            snprintf(wpnBuf, sizeof(wpnBuf), "---");
        }
        ImU32 wTextColor = isActiveWeapon ? IM_COL32(0, 220, 210, 220)
                                          : IM_COL32(80, 90, 100, 180);
        draw->AddText(ImVec2(wx + 6.0f, sy + 4.0f), wTextColor, wpnBuf);
    }

    if (smallFont) ImGui::PopFont();
}

static void RenderHUD_Degradation(ImDrawList* draw, float alertRatio, float globalTime,
                                   float vpW, float vpH, float vpX, float vpY) {
    if (alertRatio > 1.0f) alertRatio = 1.0f;  // 防止超出范围导致过量绘制
    // 噪点层（alertRatio > 0.2）
    if (alertRatio > 0.2f) {
        float noiseDensity = (alertRatio - 0.2f) * 60.0f;
        int dotCount = (int)noiseDensity;
        for (int i = 0; i < dotCount; ++i) {
            float seed = (float)i * 73.0f + globalTime * 37.0f;
            float nx = vpX + vpW * (0.05f + 0.9f * (sinf(seed) * 0.5f + 0.5f));
            float ny = vpY + vpH * (0.05f + 0.9f * (cosf(seed * 1.3f) * 0.5f + 0.5f));
            uint8_t alpha = (uint8_t)(20.0f + alertRatio * 40.0f);
            draw->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + 2.0f, ny + 2.0f),
                                IM_COL32(0, 200, 190, alpha));
        }
    }

    // 缺角线条（alertRatio > 0.4）
    if (alertRatio > 0.4f) {
        float glitchIntensity = (alertRatio - 0.4f) * 3.0f;
        int lineCount = (int)(glitchIntensity * 4.0f);
        for (int i = 0; i < lineCount; ++i) {
            float seed = (float)i * 131.0f + globalTime * 23.0f;
            float ly = vpY + vpH * (sinf(seed) * 0.5f + 0.5f);
            float lxStart = vpX + vpW * (cosf(seed * 0.7f) * 0.3f + 0.3f);
            float lxEnd = lxStart + 40.0f + 60.0f * sinf(seed * 1.7f);
            uint8_t alpha = (uint8_t)(15.0f + glitchIntensity * 25.0f);
            draw->AddLine(ImVec2(lxStart, ly), ImVec2(lxEnd, ly),
                          IM_COL32(0, 180, 170, alpha), 1.0f);
        }
    }

    // 扫描线加重（alertRatio > 0.66）
    if (alertRatio > 0.66f) {
        float scanIntensity = (alertRatio - 0.66f) * 3.0f;
        float scrollOffset = fmodf(globalTime * 30.0f, 4.0f);
        uint8_t scanAlpha = (uint8_t)(8.0f + scanIntensity * 20.0f);

        for (float y = vpY + scrollOffset; y < vpY + vpH; y += 4.0f) {
            draw->AddLine(ImVec2(vpX, y), ImVec2(vpX + vpW, y),
                          IM_COL32(0, 0, 0, scanAlpha), 1.0f);
        }
    }
}

// ============================================================
// RenderHUD_NoiseIndicator — 噪音指示器（左下）
// ============================================================

static void RenderHUD_NoiseIndicator(ImDrawList* draw, float x, float y,
                                      float noiseLevel, float globalTime) {
    if (noiseLevel < 0.01f) return;

    ImFont* smallFont = UITheme::GetFont_Small();
    if (smallFont) ImGui::PushFont(smallFont);

    // 标签
    draw->AddText(ImVec2(x, y - 2.0f), IM_COL32(0, 140, 130, 150), "NOISE");

    if (smallFont) ImGui::PopFont();

    // 声波扩散圆环
    float cx = x + 50.0f;
    float cy = y + 8.0f;
    float maxR = 16.0f;

    int ringCount = 1 + (int)(noiseLevel * 3.0f); // 1~4 个环
    for (int i = 0; i < ringCount; ++i) {
        float phase = globalTime * 3.0f + (float)i * 0.8f;
        float expandT = fmodf(phase, 1.5f) / 1.5f;  // [0,1) 循环
        float r = 4.0f + expandT * maxR * noiseLevel;
        uint8_t alpha = (uint8_t)((1.0f - expandT) * 180.0f * noiseLevel);

        ImU32 ringColor;
        if (noiseLevel < 0.3f)       ringColor = IM_COL32(0, 200, 190, alpha);
        else if (noiseLevel < 0.6f)  ringColor = IM_COL32(220, 200, 0, alpha);
        else                          ringColor = IM_COL32(220, 60, 20, alpha);

        draw->AddCircle(ImVec2(cx, cy), r, ringColor, 24, 1.5f);
    }

    // 中心点
    uint8_t dotAlpha = (uint8_t)(120.0f + noiseLevel * 135.0f);
    ImU32 dotColor;
    if (noiseLevel < 0.3f)       dotColor = IM_COL32(0, 200, 190, dotAlpha);
    else if (noiseLevel < 0.6f)  dotColor = IM_COL32(220, 200, 0, dotAlpha);
    else                          dotColor = IM_COL32(220, 60, 20, dotAlpha);
    draw->AddCircleFilled(ImVec2(cx, cy), 3.0f, dotColor);
}

// ============================================================
// RenderHUD — 游戏内 HUD 主入口
// ============================================================

void RenderHUD(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_GameplayState>()) return;
    auto& gs = registry.ctx<Res_GameplayState>();
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    // 左右分屏：游戏区域 = 视口宽度 - 聊天面板宽度
    float gameW = vpSize.x;
    if (registry.has_ctx<Res_ChatState>()) {
        gameW -= Res_ChatState::PANEL_WIDTH;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    const float pad = 20.0f;

    // 左上: 任务面板
    RenderHUD_MissionPanel(draw, vpPos.x + pad, vpPos.y + pad,
                           gs.missionName, gs.objectiveText);

    // 右上（游戏区域内）: 警戒度仪表
    const float alertW = 240.0f;
    const float alertH = 52.0f;
    RenderHUD_AlertGauge(draw,
        vpPos.x + gameW - alertW - pad, vpPos.y + pad,
        alertW, alertH, gs.alertLevel, gs.alertMax);

    // 游戏区域上方中央: 倒计时
    RenderHUD_Countdown(draw,
        vpPos.x + gameW * 0.5f, vpPos.y + pad,
        gs.countdownTimer, gs.countdownActive);

    // 左下: 玩家状态
    RenderHUD_PlayerState(draw,
        vpPos.x + pad, vpPos.y + vpSize.y - 50.0f,
        static_cast<uint8_t>(gs.playerMoveState), gs.playerDisguised);

    // 左下偏右: 噪音指示器
    RenderHUD_NoiseIndicator(draw,
        vpPos.x + pad + 180.0f, vpPos.y + vpSize.y - 50.0f,
        gs.noiseLevel, ui.globalTime);

    // 游戏区域右下: 道具/武器快捷栏
    RenderHUD_ItemSlots(draw,
        vpPos.x + gameW - 280.0f, vpPos.y + vpSize.y - 60.0f,
        gs);

    // 右侧聊天面板（独立区域）
    RenderChatPanel(registry, 0.0f);

    // 道具快捷轮盘（Tab 按住时叠加在游戏区域中央）
    RenderItemWheel(registry, 0.0f);

    // UI 退化效果（仅覆盖游戏区域）
    float alertRatio = (gs.alertMax > 0.0f) ? gs.alertLevel / gs.alertMax : 0.0f;
    if (alertRatio > 0.2f) {
        RenderHUD_Degradation(draw, alertRatio, ui.globalTime,
                              gameW, vpSize.y, vpPos.x, vpPos.y);
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
