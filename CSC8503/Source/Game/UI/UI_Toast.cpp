#include "UI_Toast.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cstdio>
#include <algorithm>

#include "Game/UI/UITheme.h"

namespace ECS::UI {

using namespace ECS::UITheme;

// ============================================================
// PushToast — 写入环形缓冲的下一个槽位
// ============================================================

void PushToast(Registry& registry, const char* text, ToastType type, float duration) {
    if (!registry.has_ctx<Res_ToastState>()) return;
    auto& state = registry.ctx<Res_ToastState>();

    ToastEntry& entry = state.toasts[state.nextSlot];
    std::snprintf(entry.text, sizeof(entry.text), "%s", text);
    entry.lifetime = duration;
    entry.elapsed  = 0.0f;
    entry.type     = type;
    entry.active   = true;

    state.nextSlot = (state.nextSlot + 1) % Res_ToastState::kMaxToasts;
}

// ============================================================
// RenderToasts — ForegroundDrawList 覆盖渲染
// ============================================================

void RenderToasts(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_ToastState>()) return;
    auto& state = registry.ctx<Res_ToastState>();

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImFont* font = GetFont_Small();
    if (!font) return;

    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    constexpr float kPaddingX    = 12.0f;
    constexpr float kPaddingY    = 8.0f;
    constexpr float kMarginRight = 16.0f;
    constexpr float kMarginTop   = 16.0f;
    constexpr float kSpacing     = 8.0f;
    constexpr float kRounding    = 6.0f;
    constexpr float kBorderWidth = 1.0f;
    constexpr float kFadeIn      = 0.3f;
    constexpr float kFadeOut     = 0.5f;

    float yOffset = kMarginTop;

    for (int i = 0; i < Res_ToastState::kMaxToasts; ++i) {
        ToastEntry& entry = state.toasts[i];
        if (!entry.active) continue;

        entry.elapsed += dt;
        if (entry.elapsed >= entry.lifetime) {
            entry.active = false;
            continue;
        }

        // ── Alpha + slide 计算：淡入(+右滑入) → 持续 → 淡出 ──
        float alpha = 1.0f;
        float slideOff = 0.0f;     // 右侧滑入偏移
        constexpr float kSlideInDist = 20.0f;

        if (entry.elapsed < kFadeIn) {
            float t = entry.elapsed / kFadeIn;
            alpha    = t;
            slideOff = (1.0f - t) * kSlideInDist;
        } else if (entry.elapsed > entry.lifetime - kFadeOut) {
            alpha = (entry.lifetime - entry.elapsed) / kFadeOut;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        const ImU32 bgAlpha     = static_cast<ImU32>(230 * alpha);
        const ImU32 borderAlpha = static_cast<ImU32>(180 * alpha);
        const ImU32 textAlpha   = static_cast<ImU32>(255 * alpha);

        // ── 背景 & 边框颜色（项目 5 色标准）──
        const ImU32 bgColor     = Col32_Bg(bgAlpha);
        const ImU32 borderColor = Col32_Gray(borderAlpha);

        // ── 文字颜色：Info = 近黑，Success = 橙色，Warning = 橙色暗，Danger = 红 ──
        ImU32 textColor;
        ImU32 iconColor;
        switch (entry.type) {
            case ToastType::Success:
                textColor = Col32_Accent(textAlpha);
                iconColor = Col32_Accent(textAlpha);
                break;
            case ToastType::Warning:
                textColor = IM_COL32(200, 150, 30, textAlpha);
                iconColor = IM_COL32(200, 150, 30, textAlpha);
                break;
            case ToastType::Danger:
                textColor = IM_COL32(200, 50, 50, textAlpha);
                iconColor = IM_COL32(200, 50, 50, textAlpha);
                break;
            case ToastType::Info:
            default:
                textColor = Col32_Text(textAlpha);
                iconColor = Col32_Gray(textAlpha);
                break;
        }

        // ── 计算文字尺寸与矩形位置 ──
        ImGui::PushFont(font);
        const float fontSize = ImGui::GetFontSize();
        const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, entry.text);

        constexpr float kIconSpace = 16.0f;  // 图标占位
        const float boxW = textSize.x + kPaddingX * 2.0f + kIconSpace;
        const float boxH = textSize.y + kPaddingY * 2.0f;

        const float x1 = displaySize.x - kMarginRight - boxW + slideOff;
        const float y1 = yOffset;
        const float x2 = displaySize.x - kMarginRight + slideOff;
        const float y2 = yOffset + boxH;

        // ── 绘制圆角背景 + 边框 ──
        drawList->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), bgColor, kRounding);
        drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), borderColor, kRounding, 0, kBorderWidth);

        // ── 类型图标 ──
        float iconX = x1 + kPaddingX + 4.0f;
        float iconY = y1 + boxH * 0.5f;
        switch (entry.type) {
            case ToastType::Info:
                // Circle
                drawList->AddCircle(ImVec2(iconX, iconY), 4.0f, iconColor, 12, 1.5f);
                break;
            case ToastType::Success:
                // Checkmark
                drawList->AddLine(ImVec2(iconX - 3.0f, iconY), ImVec2(iconX - 1.0f, iconY + 3.0f), iconColor, 1.5f);
                drawList->AddLine(ImVec2(iconX - 1.0f, iconY + 3.0f), ImVec2(iconX + 4.0f, iconY - 3.0f), iconColor, 1.5f);
                break;
            case ToastType::Warning:
                // Triangle
                drawList->AddTriangle(
                    ImVec2(iconX, iconY - 4.0f),
                    ImVec2(iconX - 4.0f, iconY + 3.0f),
                    ImVec2(iconX + 4.0f, iconY + 3.0f),
                    iconColor, 1.5f);
                break;
            case ToastType::Danger:
                // Diamond
                drawList->AddLine(ImVec2(iconX, iconY - 4.0f), ImVec2(iconX + 4.0f, iconY), iconColor, 1.5f);
                drawList->AddLine(ImVec2(iconX + 4.0f, iconY), ImVec2(iconX, iconY + 4.0f), iconColor, 1.5f);
                drawList->AddLine(ImVec2(iconX, iconY + 4.0f), ImVec2(iconX - 4.0f, iconY), iconColor, 1.5f);
                drawList->AddLine(ImVec2(iconX - 4.0f, iconY), ImVec2(iconX, iconY - 4.0f), iconColor, 1.5f);
                break;
        }

        // ── 绘制文字 ──
        drawList->AddText(font, fontSize,
                          ImVec2(x1 + kPaddingX + kIconSpace, y1 + kPaddingY),
                          textColor, entry.text);

        ImGui::PopFont();

        yOffset += boxH + kSpacing;
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
