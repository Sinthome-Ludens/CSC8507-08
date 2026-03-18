/**
 * @file UI_ActionNotify.cpp
 * @brief 动作通知系统实现（右上角圆角卡片 + 积分同步）。
 */
#include "UI_ActionNotify.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cstdio>
#include "Game/Components/Res_ActionNotifyState.h"

namespace ECS::UI {

static constexpr float kFadeOut = 0.4f;   ///< 淡出时长（秒）

// ── 颜色映射 ─────────────────────────────────────────────────
/**
 * @brief 根据通知类型返回对应的 RGBA 颜色值。
 * @param type 通知类型（Kill/ItemPickup/Weapon/Bonus/Alert）
 * @param a    Alpha 通道（0–255）
 * @return ImU32 IM_COL32 格式颜色
 */
static ImU32 TypeColor(ActionNotifyType type, uint8_t a) {
    switch (type) {
        case ActionNotifyType::Kill:       return IM_COL32(200,  50,  50, a);
        case ActionNotifyType::ItemPickup: return IM_COL32( 80, 180, 100, a);
        case ActionNotifyType::Weapon:     return IM_COL32( 30, 180, 200, a);
        case ActionNotifyType::Bonus:      return IM_COL32(252, 111,  41, a);
        case ActionNotifyType::Alert:      return IM_COL32(200, 150,  30, a);
        default:                           return IM_COL32(200, 200, 200, a);
    }
}

/** @brief 推入一条动作通知（纯通知，不修改积分）。 */
void PushActionNotify(Registry& registry, const char* verb, const char* target,
                      int scoreDelta, ActionNotifyType type, float lifetime) {
    if (!registry.has_ctx<Res_ActionNotifyState>()) return;
    auto& state = registry.ctx<Res_ActionNotifyState>();
    auto& entry = state.entries[state.nextSlot];
    if (!verb)   verb   = "";
    if (!target) target = "";
    std::snprintf(entry.verb,   sizeof(entry.verb),   "%s", verb);
    std::snprintf(entry.target, sizeof(entry.target), "%s", target);
    entry.scoreDelta = scoreDelta;
    // B4：保证 lifetime 至少能完成一次淡出，避免 hold 为负
    entry.lifetime   = std::max(kFadeOut + 0.1f, lifetime);
    entry.elapsed    = 0.0f;
    entry.type       = type;
    entry.active     = true;
    state.nextSlot   = (state.nextSlot + 1) % Res_ActionNotifyState::kMaxEntries;
}

/** @brief 渲染右上角动作通知卡片列表（淡入 → 持续 → 淡出）。 */
void RenderActionNotify(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_ActionNotifyState>()) return;
    auto& state = registry.ctx<Res_ActionNotifyState>();

    ImDrawList* draw        = ImGui::GetForegroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    constexpr float kBoxW      = 320.0f;   // 与 Res_ChatState::kPanelWidth 对齐
    constexpr float kPadX      =  12.0f;
    constexpr float kPadY      =   7.0f;
    constexpr float kBoxH      =  34.0f;
    constexpr float kGap       =   6.0f;
    constexpr float kBarW      =   3.0f;
    constexpr float kRounding  =   6.0f;
    constexpr float kFadeIn    =   0.2f;
    // kFadeOut 已提升至文件作用域（0.4f）
    constexpr float kSlideX    =  12.0f;

    float curY = 16.0f;

    for (int i = 0; i < Res_ActionNotifyState::kMaxEntries; ++i) {
        auto& e = state.entries[i];
        if (!e.active) continue;

        // 推进计时
        e.elapsed += dt;
        if (e.elapsed >= e.lifetime) {
            e.active = false;
            continue;
        }

        // 计算 alpha + 水平滑入
        float alpha  = 1.0f;
        float slideOff = 0.0f;
        float hold = e.lifetime - kFadeOut;

        if (e.elapsed < kFadeIn) {
            float t = e.elapsed / kFadeIn;
            alpha    = t;
            slideOff = (1.0f - t) * kSlideX;
        } else if (e.elapsed < hold) {
            alpha    = 1.0f;
            slideOff = 0.0f;
        } else {
            float t  = (e.elapsed - hold) / kFadeOut;
            alpha    = 1.0f - std::min(t, 1.0f);
            slideOff = 0.0f;
        }

        uint8_t a = static_cast<uint8_t>(255.0f * alpha);

        float boxX = displaySize.x - 16.0f - kBoxW + slideOff;
        float boxY = curY;

        // 卡片背景
        draw->AddRectFilled(
            ImVec2(boxX, boxY),
            ImVec2(boxX + kBoxW, boxY + kBoxH),
            IM_COL32(245, 238, 232, static_cast<uint8_t>(220.0f * alpha)),
            kRounding);

        // 卡片边框
        draw->AddRect(
            ImVec2(boxX, boxY),
            ImVec2(boxX + kBoxW, boxY + kBoxH),
            IM_COL32(200, 200, 200, static_cast<uint8_t>(180.0f * alpha)),
            kRounding, 0, 1.0f);

        // 左侧彩色竖条
        draw->AddRectFilled(
            ImVec2(boxX, boxY + kRounding),
            ImVec2(boxX + kBarW, boxY + kBoxH - kRounding),
            TypeColor(e.type, a));

        // 文字基线
        float textX = boxX + kBarW + kPadX;
        float textY = boxY + kPadY;

        // B3：裁剪至卡片内容区，防止超长文本溢出右边界
        float clipMaxX = boxX + kBoxW - kPadX;
        draw->PushClipRect(ImVec2(textX, boxY), ImVec2(clipMaxX, boxY + kBoxH), true);

        // 动词（近黑）
        draw->AddText(ImVec2(textX, textY),
            IM_COL32(16, 13, 10, a), e.verb);

        // 目标名（类型色）— 紧跟动词后
        ImVec2 verbSize = ImGui::CalcTextSize(e.verb);
        float targetX = textX + verbSize.x + 4.0f;
        draw->AddText(ImVec2(targetX, textY),
            TypeColor(e.type, a), e.target);

        // 分值（非零才显示）
        if (e.scoreDelta != 0) {
            ImVec2 targetSize = ImGui::CalcTextSize(e.target);
            float scoreX = targetX + targetSize.x + 6.0f;
            char scoreBuf[16];
            if (e.scoreDelta > 0) {
                std::snprintf(scoreBuf, sizeof(scoreBuf), "+%d", e.scoreDelta);
                draw->AddText(ImVec2(scoreX, textY),
                    IM_COL32(252, 111, 41, a), scoreBuf);
            } else {
                std::snprintf(scoreBuf, sizeof(scoreBuf), "%d", e.scoreDelta);
                draw->AddText(ImVec2(scoreX, textY),
                    IM_COL32(200, 50, 50, a), scoreBuf);
            }
        }

        draw->PopClipRect();  // B3：恢复裁剪区

        curY += kBoxH + kGap;
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
