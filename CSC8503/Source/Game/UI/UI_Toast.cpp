#include "UI_Toast.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/Res_ToastState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// 辅助：EaseOutCubic 缓动
// ============================================================

static float EaseOutCubic(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}

// ============================================================
// 辅助：按 ToastType 返回类型色
// ============================================================

static ImU32 GetTypeColor(ToastType type, float alpha) {
    uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
    switch (type) {
        case ToastType::Info:    return IM_COL32(0,   217, 204, a);  // 青色
        case ToastType::Warning: return IM_COL32(230, 178, 51,  a);  // 琥珀
        case ToastType::Danger:  return IM_COL32(230, 64,  51,  a);  // 红色
        case ToastType::Success: return IM_COL32(76,  204, 76,  a);  // 绿色
        default:                 return IM_COL32(0,   217, 204, a);
    }
}

// ============================================================
// 辅助：按 ToastType 返回图标前缀
// ============================================================

static const char* GetTypeIcon(ToastType type) {
    switch (type) {
        case ToastType::Info:    return "[i] ";
        case ToastType::Warning: return "[!] ";
        case ToastType::Danger:  return "[X] ";
        case ToastType::Success: return "[+] ";
        default:                 return "[i] ";
    }
}

// ============================================================
// RenderToasts — 主渲染函数
// ============================================================

void RenderToasts(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_ToastState>()) return;
    const auto& toast = registry.ctx<Res_ToastState>();

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // ── 布局参数 ──
    constexpr float CARD_W      = 300.0f;
    constexpr float CARD_H      = 36.0f;
    constexpr float GAP         = 6.0f;
    constexpr float STRIPE_W    = 4.0f;
    constexpr float START_Y     = 82.0f;   // mission panel 下方
    constexpr float MARGIN_LEFT = 12.0f;
    constexpr float SLIDE_DIST  = 320.0f;  // 滑入距离

    // 动画时间常量
    constexpr float SLIDE_IN_DUR = 0.25f;
    constexpr float FADE_IN_DUR  = 0.15f;
    constexpr float FADE_OUT_DUR = 0.5f;

    float curY = START_Y;

    // O(n) 单次遍历环形缓冲区（从最旧→最新）
    for (int k = 0; k < Res_ToastState::MAX_TOASTS; ++k) {
        int idx = (toast.head + k) % Res_ToastState::MAX_TOASTS;
        const auto& e = toast.entries[idx];
        if (!e.active) continue;

        float age  = e.age;
        float life = e.lifespan;
        float maxL = e.maxLife;

        // ── 计算 alpha ──
        float alpha = 1.0f;

        // 淡入阶段
        if (age < FADE_IN_DUR) {
            alpha = age / FADE_IN_DUR;
        }
        // 淡出阶段（最后 FADE_OUT_DUR 秒）
        if (life < FADE_OUT_DUR && maxL > 0.0f) {
            float fadeAlpha = life / FADE_OUT_DUR;
            if (fadeAlpha < alpha) alpha = fadeAlpha;
        }
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        // ── 计算滑入偏移 ──
        float slideT = (age < SLIDE_IN_DUR) ? (age / SLIDE_IN_DUR) : 1.0f;
        float slideOffset = (1.0f - EaseOutCubic(slideT)) * SLIDE_DIST;

        // ── 塌缩效果（淡出阶段卡片高度收缩）──
        float heightScale = 1.0f;
        if (life < FADE_OUT_DUR && maxL > 0.0f) {
            heightScale = life / FADE_OUT_DUR;
            if (heightScale < 0.0f) heightScale = 0.0f;
        }
        float cardH = CARD_H * heightScale;
        float gap   = GAP * heightScale;

        // ── 卡片位置 ──
        float x = MARGIN_LEFT - slideOffset;
        float y = curY;

        // ── 绘制半透明深色背景 ──
        uint8_t bgAlpha = static_cast<uint8_t>(alpha * 200.0f);
        draw->AddRectFilled(
            ImVec2(x, y), ImVec2(x + CARD_W, y + cardH),
            IM_COL32(10, 12, 20, bgAlpha), 3.0f);

        // ── 绘制左侧类型色条 ──
        ImU32 stripeColor = GetTypeColor(e.type, alpha);
        draw->AddRectFilled(
            ImVec2(x, y), ImVec2(x + STRIPE_W, y + cardH),
            stripeColor, 3.0f, ImDrawFlags_RoundCornersLeft);

        // ── 绘制文本（ClipRect 裁剪到卡片范围）──
        if (cardH > 10.0f) {
            ImFont* font = UITheme::GetFont_Small();
            if (font) ImGui::PushFont(font);

            // 裁剪矩形：防止文本溢出卡片右侧
            draw->PushClipRect(ImVec2(x, y), ImVec2(x + CARD_W, y + cardH), true);

            const char* icon = GetTypeIcon(e.type);
            ImU32 iconColor = GetTypeColor(e.type, alpha * 0.9f);

            float fontH = ImGui::GetFontSize();
            float textY = y + (cardH - fontH) * 0.5f;
            float textX = x + STRIPE_W + 6.0f;
            draw->AddText(ImVec2(textX, textY), iconColor, icon);

            float iconW = ImGui::CalcTextSize(icon).x;
            uint8_t textAlpha = static_cast<uint8_t>(alpha * 230.0f);
            draw->AddText(
                ImVec2(textX + iconW, textY),
                IM_COL32(217, 230, 235, textAlpha),
                e.text);

            draw->PopClipRect();

            if (font) ImGui::PopFont();
        }

        curY += cardH + gap;
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
