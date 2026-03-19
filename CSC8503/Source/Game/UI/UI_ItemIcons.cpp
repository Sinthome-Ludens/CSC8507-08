/**
 * @file UI_ItemIcons.cpp
 * @brief 5 种道具几何图标实现（ImDrawList 原语绘制）。
 */
#include "UI_ItemIcons.h"
#ifdef USE_IMGUI

#include <cmath>
#include "Game/UI/UITheme.h"

namespace ECS::UI {
using namespace ECS::UITheme;

/// HoloBait: 六角棱镜 + 3 条中心辐射线
static void DrawIcon_HoloBait(ImDrawList* draw, ImVec2 c, float r, ImU32 col) {
    // Hexagon outline
    ImVec2 pts[6];
    for (int i = 0; i < 6; ++i) {
        float a = kPI / 3.0f * i - kPI / 6.0f;
        pts[i] = ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r);
    }
    for (int i = 0; i < 6; ++i)
        draw->AddLine(pts[i], pts[(i + 1) % 6], col, 1.5f);
    // 3 radial lines (alternating vertices)
    for (int i = 0; i < 6; i += 2)
        draw->AddLine(c, pts[i], col, 1.0f);
}

/// PhotonRadar: 3 层同心弧 + 中心圆点
static void DrawIcon_PhotonRadar(ImDrawList* draw, ImVec2 c, float r, ImU32 col) {
    draw->AddCircleFilled(c, r * 0.15f, col, 8);
    for (int ring = 1; ring <= 3; ++ring) {
        float radius = r * ring / 3.0f;
        // Draw arc from -60 to +60 degrees (120 degree sweep)
        int segments = 12;
        float startA = -kPI / 3.0f;
        float endA   = kPI / 3.0f;
        float da = (endA - startA) / segments;
        for (int s = 0; s < segments; ++s) {
            float a1 = startA + s * da - kPI / 2.0f;
            float a2 = a1 + da;
            draw->AddLine(
                ImVec2(c.x + cosf(a1) * radius, c.y + sinf(a1) * radius),
                ImVec2(c.x + cosf(a2) * radius, c.y + sinf(a2) * radius),
                col, 1.5f);
        }
    }
}

/// DDoS: 菱形轮廓 + Z 字折线穿过
static void DrawIcon_DDoS(ImDrawList* draw, ImVec2 c, float r, ImU32 col) {
    // Diamond
    ImVec2 top(c.x, c.y - r);
    ImVec2 right(c.x + r, c.y);
    ImVec2 bottom(c.x, c.y + r);
    ImVec2 left(c.x - r, c.y);
    draw->AddLine(top, right, col, 1.5f);
    draw->AddLine(right, bottom, col, 1.5f);
    draw->AddLine(bottom, left, col, 1.5f);
    draw->AddLine(left, top, col, 1.5f);
    // Z-shape lightning bolt through center
    float zw = r * 0.4f;
    draw->AddLine(ImVec2(c.x - zw, c.y - r * 0.5f), ImVec2(c.x + zw, c.y - r * 0.5f), col, 1.5f);
    draw->AddLine(ImVec2(c.x + zw, c.y - r * 0.5f), ImVec2(c.x - zw, c.y + r * 0.5f), col, 1.5f);
    draw->AddLine(ImVec2(c.x - zw, c.y + r * 0.5f), ImVec2(c.x + zw, c.y + r * 0.5f), col, 1.5f);
}

/// RoamAI: 等腰三角 + 圆形"眼" + 天线
static void DrawIcon_RoamAI(ImDrawList* draw, ImVec2 c, float r, ImU32 col) {
    // Triangle (pointing up)
    ImVec2 t(c.x, c.y - r);
    ImVec2 bl(c.x - r * 0.8f, c.y + r * 0.6f);
    ImVec2 br(c.x + r * 0.8f, c.y + r * 0.6f);
    draw->AddLine(t, bl, col, 1.5f);
    draw->AddLine(bl, br, col, 1.5f);
    draw->AddLine(br, t, col, 1.5f);
    // Eye
    draw->AddCircle(ImVec2(c.x, c.y + r * 0.1f), r * 0.2f, col, 8, 1.5f);
    // Antenna
    draw->AddLine(t, ImVec2(c.x, c.y - r * 1.3f), col, 1.0f);
    draw->AddCircleFilled(ImVec2(c.x, c.y - r * 1.3f), r * 0.1f, col, 6);
}

/// TargetStrike: 十字准星 + 小圆 + 4 个角括号
static void DrawIcon_TargetStrike(ImDrawList* draw, ImVec2 c, float r, ImU32 col) {
    // Crosshair lines (with gap)
    float gap = r * 0.25f;
    draw->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y - gap), col, 1.5f);
    draw->AddLine(ImVec2(c.x, c.y + gap), ImVec2(c.x, c.y + r), col, 1.5f);
    draw->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x - gap, c.y), col, 1.5f);
    draw->AddLine(ImVec2(c.x + gap, c.y), ImVec2(c.x + r, c.y), col, 1.5f);
    // Center circle
    draw->AddCircle(c, r * 0.2f, col, 8, 1.5f);
    // 4 corner brackets
    float br = r * 0.85f;
    float bl = r * 0.3f;
    // Top-left
    draw->AddLine(ImVec2(c.x - br, c.y - br), ImVec2(c.x - br, c.y - br + bl), col, 1.5f);
    draw->AddLine(ImVec2(c.x - br, c.y - br), ImVec2(c.x - br + bl, c.y - br), col, 1.5f);
    // Top-right
    draw->AddLine(ImVec2(c.x + br, c.y - br), ImVec2(c.x + br, c.y - br + bl), col, 1.5f);
    draw->AddLine(ImVec2(c.x + br, c.y - br), ImVec2(c.x + br - bl, c.y - br), col, 1.5f);
    // Bottom-left
    draw->AddLine(ImVec2(c.x - br, c.y + br), ImVec2(c.x - br, c.y + br - bl), col, 1.5f);
    draw->AddLine(ImVec2(c.x - br, c.y + br), ImVec2(c.x - br + bl, c.y + br), col, 1.5f);
    // Bottom-right
    draw->AddLine(ImVec2(c.x + br, c.y + br), ImVec2(c.x + br, c.y + br - bl), col, 1.5f);
    draw->AddLine(ImVec2(c.x + br, c.y + br), ImVec2(c.x + br - bl, c.y + br), col, 1.5f);
}

/// GlobalMap: 圆形外框 + 十字准线 + 中心点 + 北方三角指示器
static void DrawIcon_GlobalMap(ImDrawList* draw, ImVec2 c, float r, ImU32 col) {
    // 外圆
    draw->AddCircle(c, r * 0.9f, col, 24, 1.5f);
    // 十字准线
    draw->AddLine(ImVec2(c.x - r * 0.5f, c.y),
                  ImVec2(c.x + r * 0.5f, c.y), col, 1.0f);
    draw->AddLine(ImVec2(c.x, c.y - r * 0.5f),
                  ImVec2(c.x, c.y + r * 0.5f), col, 1.0f);
    // 中心点
    draw->AddCircleFilled(c, 2.0f, col);
    // 北方指示三角（顶部）
    draw->AddTriangleFilled(
        ImVec2(c.x, c.y - r * 0.85f),
        ImVec2(c.x - 2.5f, c.y - r * 0.6f),
        ImVec2(c.x + 2.5f, c.y - r * 0.6f), col);
}

void DrawItemIcon(ImDrawList* draw, ImVec2 center, float size, ItemID id, ImU32 color) {
    switch (id) {
        case ItemID::HoloBait:     DrawIcon_HoloBait(draw, center, size, color);     break;
        case ItemID::PhotonRadar:  DrawIcon_PhotonRadar(draw, center, size, color);  break;
        case ItemID::DDoS:         DrawIcon_DDoS(draw, center, size, color);         break;
        case ItemID::RoamAI:       DrawIcon_RoamAI(draw, center, size, color);       break;
        case ItemID::TargetStrike: DrawIcon_TargetStrike(draw, center, size, color); break;
        case ItemID::GlobalMap:    DrawIcon_GlobalMap(draw, center, size, color);     break;
        default: break;
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
