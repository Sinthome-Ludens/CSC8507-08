#include "UI_Interaction.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstring>
#include <algorithm>

#include "Window.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Interactable.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/UI/UITheme.h"
#include "Matrix.h"
#include "Camera.h"

// forward-declare GameWorld to access GetMainCamera()
#include "GameWorld.h"

namespace ECS::UI {

// ============================================================
// 内部辅助
// ============================================================

/// 根据 InteractionType 返回默认动作文本（不含按键前缀）
static const char* GetDefaultActionText(InteractionType type) {
    switch (type) {
        case InteractionType::PickUp:    return "PICK UP";
        case InteractionType::Use:       return "USE";
        case InteractionType::Hack:      return "HACK";
        case InteractionType::Eliminate: return "ELIMINATE";
        case InteractionType::Examine:   return "EXAMINE";
        default:                         return "INTERACT";
    }
}

/// 根据 InteractionType 返回左侧色条颜色
static ImU32 GetTypeAccentColor(InteractionType type, float alpha) {
    uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
    switch (type) {
        case InteractionType::PickUp:    return IM_COL32(0,   217, 204, a); // 青
        case InteractionType::Use:       return IM_COL32(80,  200, 120, a); // 绿
        case InteractionType::Hack:      return IM_COL32(255, 170,  50, a); // 橙
        case InteractionType::Eliminate: return IM_COL32(220,  60,  40, a); // 红
        case InteractionType::Examine:   return IM_COL32(180, 120, 230, a); // 紫
        default:                         return IM_COL32(0,   217, 204, a);
    }
}

/// 缓存近距离实体，按距离排序后渲染（最多 kMaxPrompts 个）
struct PromptEntry {
    NCL::Maths::Vector3 worldPos;
    float               distance;
    float               offsetY;
    float               radius;
    InteractionType     type;
    char                displayText[32]; // "[E] XXX"
};

static constexpr int kMaxPrompts = 8;

// ============================================================
// RenderInteractionPrompts
// ============================================================

void RenderInteractionPrompts(Registry& registry, float /*dt*/) {
    // ── 安全检查：需要 NCL Bridge 指针 ──
    if (!registry.has_ctx<Res_NCL_Pointers>()) return;
    auto& nclPtrs = registry.ctx<Res_NCL_Pointers>();
    if (!nclPtrs.world) return;

    auto* win = NCL::Window::GetWindow();
    if (!win) return;

    // ── 获取相机 VP 矩阵 ──
    auto& cam = nclPtrs.world->GetMainCamera();
    NCL::Maths::Matrix4 viewMat = cam.BuildViewMatrix();
    NCL::Maths::Matrix4 projMat = cam.BuildProjectionMatrix(win->GetScreenAspect());
    NCL::Maths::Matrix4 vpMat   = projMat * viewMat;

    NCL::Maths::Vector3 camPos = cam.GetPosition();

    // ── ImGui viewport 信息 ──
    ImVec2 vpPos  = ImGui::GetMainViewport()->Pos;
    ImVec2 vpSize = ImGui::GetMainViewport()->Size;

    // ── 收集范围内的可交互实体 ──
    PromptEntry entries[kMaxPrompts];
    int count = 0;

    auto view = registry.view<C_D_Transform, C_D_Interactable>();
    view.each([&](EntityID /*id*/, const C_D_Transform& tf, const C_D_Interactable& inter) {
        if (!inter.enabled) return;
        if (count >= kMaxPrompts) return;

        float dx = tf.position.x - camPos.x;
        float dy = tf.position.y - camPos.y;
        float dz = tf.position.z - camPos.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (dist > inter.radius) return;

        auto& e = entries[count];
        e.worldPos = tf.position;
        e.distance = dist;
        e.offsetY  = inter.offsetY;
        e.radius   = inter.radius;
        e.type     = inter.type;

        // 构造显示文本: "[E] ACTION" 或 "[E] CUSTOM_LABEL"
        if (inter.label[0] != '\0') {
            std::snprintf(e.displayText, sizeof(e.displayText), "[E] %s", inter.label);
        } else {
            std::snprintf(e.displayText, sizeof(e.displayText), "[E] %s", GetDefaultActionText(inter.type));
        }

        ++count;
    });

    if (count == 0) return;

    // ── 按距离排序（最近的最前面，alpha 最高）──
    std::sort(entries, entries + count, [](const PromptEntry& a, const PromptEntry& b) {
        return a.distance < b.distance;
    });

    // ── 渲染每个提示 ──
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImFont* termFont  = UITheme::GetFont_Terminal();
    ImFont* smallFont = UITheme::GetFont_Small();

    for (int i = 0; i < count; ++i) {
        const auto& e = entries[i];

        // 世界坐标 → 齐次裁剪坐标
        float wx = e.worldPos.x;
        float wy = e.worldPos.y + e.offsetY;
        float wz = e.worldPos.z;

        NCL::Maths::Vector4 clipPos = vpMat * NCL::Maths::Vector4(wx, wy, wz, 1.0f);

        // 背面剔除（相机后方）
        if (clipPos.w <= 0.0f) continue;

        // 透视除法 → NDC
        float ndcX = clipPos.x / clipPos.w;
        float ndcY = clipPos.y / clipPos.w;

        // NDC [-1,1] → 屏幕像素
        float screenX = vpPos.x + (ndcX * 0.5f + 0.5f) * vpSize.x;
        float screenY = vpPos.y + (1.0f - (ndcY * 0.5f + 0.5f)) * vpSize.y; // Y 翻转

        // 超出屏幕范围 → 跳过
        if (screenX < vpPos.x - 100.0f || screenX > vpPos.x + vpSize.x + 100.0f) continue;
        if (screenY < vpPos.y - 100.0f || screenY > vpPos.y + vpSize.y + 100.0f) continue;

        // ── 距离驱动的 alpha 淡入/淡出 ──
        float fadeStart = e.radius * 0.7f;
        float alpha = 1.0f;
        if (e.distance > fadeStart) {
            alpha = 1.0f - (e.distance - fadeStart) / (e.radius - fadeStart);
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        if (alpha < 0.01f) continue;

        // ── 计算文本尺寸 ──
        ImFont* font = termFont ? termFont : ImGui::GetFont();
        if (termFont) ImGui::PushFont(termFont);

        // 三角标记 "▸ "
        const char* marker = "\xe2\x96\xb8 "; // UTF-8 for ▸
        ImVec2 markerSize = ImGui::CalcTextSize(marker);
        ImVec2 textSize   = ImGui::CalcTextSize(e.displayText);

        if (termFont) ImGui::PopFont();

        float totalTextW = markerSize.x + textSize.x;
        float padX = 10.0f;
        float padY = 6.0f;
        float accentW = 4.0f; // 左侧色条宽度

        float boxW = accentW + padX + totalTextW + padX;
        float boxH = padY + textSize.y + padY;

        // 居中于屏幕投影点上方
        float boxX = screenX - boxW * 0.5f;
        float boxY = screenY - boxH - 8.0f; // 上方偏移 8px

        // ── 绘制背景面板 ──
        uint8_t bgAlpha = static_cast<uint8_t>(200.0f * alpha);
        draw->AddRectFilled(
            ImVec2(boxX, boxY),
            ImVec2(boxX + boxW, boxY + boxH),
            IM_COL32(8, 12, 20, bgAlpha),
            3.0f
        );

        // 外框微光
        uint8_t borderAlpha = static_cast<uint8_t>(80.0f * alpha);
        draw->AddRect(
            ImVec2(boxX, boxY),
            ImVec2(boxX + boxW, boxY + boxH),
            IM_COL32(0, 100, 95, borderAlpha),
            3.0f, 0, 1.0f
        );

        // ── 左侧色条（按交互类型着色）──
        ImU32 accentCol = GetTypeAccentColor(e.type, alpha);
        draw->AddRectFilled(
            ImVec2(boxX, boxY + 2.0f),
            ImVec2(boxX + accentW, boxY + boxH - 2.0f),
            accentCol,
            2.0f
        );

        // ── 绘制文本 ──
        if (termFont) ImGui::PushFont(termFont);

        float textX = boxX + accentW + padX;
        float textY = boxY + padY;

        // 三角标记（青色）
        uint8_t cyanAlpha = static_cast<uint8_t>(220.0f * alpha);
        draw->AddText(ImVec2(textX, textY), IM_COL32(0, 217, 204, cyanAlpha), marker);

        // "[E]" 部分高亮青色 + 动作文本白色
        float afterMarkerX = textX + markerSize.x;

        // 找到 "] " 的位置来分割按键和动作文本
        const char* bracketEnd = std::strchr(e.displayText, ']');
        if (bracketEnd) {
            int keyLen = static_cast<int>(bracketEnd - e.displayText + 1); // 包含 ']'
            // 按键部分 "[E]"
            draw->AddText(ImVec2(afterMarkerX, textY), IM_COL32(0, 217, 204, cyanAlpha),
                          e.displayText, e.displayText + keyLen);

            ImVec2 keySize = ImGui::CalcTextSize(e.displayText, e.displayText + keyLen);

            // 动作文本 " PICK UP"
            uint8_t whiteAlpha = static_cast<uint8_t>(230.0f * alpha);
            draw->AddText(ImVec2(afterMarkerX + keySize.x, textY),
                          IM_COL32(220, 228, 232, whiteAlpha),
                          bracketEnd + 1);
        } else {
            // fallback: 全文本白色
            uint8_t whiteAlpha = static_cast<uint8_t>(230.0f * alpha);
            draw->AddText(ImVec2(afterMarkerX, textY),
                          IM_COL32(220, 228, 232, whiteAlpha),
                          e.displayText);
        }

        if (termFont) ImGui::PopFont();
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
