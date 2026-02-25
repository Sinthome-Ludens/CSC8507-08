#include "UI_TitleScreen.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Window.h"
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"
#include "Game/Utils/Log.h"

using namespace NCL;

namespace ECS::UI {

// ============================================================
// 颜色主题（集中定义，便于后续一键换肤）
// ============================================================

static constexpr ImU32 kBgColor       = IM_COL32(18, 20, 26, 255);     // 深炭灰背景
static constexpr ImU32 kDiscColor     = IM_COL32(210, 215, 220, 255);  // 碟片主体（浅灰白）
static constexpr ImU32 kDiscGroove    = IM_COL32(120, 125, 130, 160);  // 碟片槽线
static constexpr ImU32 kDiscTick      = IM_COL32(80, 85, 90, 100);    // 碟片刻度线
static constexpr ImU32 kHubColor      = IM_COL32(30, 32, 38, 255);    // 中心hub（深色）
static constexpr ImU32 kCenterDot     = IM_COL32(220, 225, 230, 255); // 中心点（亮）
static constexpr ImU32 kDiscEdgeGlow  = IM_COL32(255, 255, 255, 40);  // 碟片边缘高光

static constexpr ImU32 kKeyUpFill     = IM_COL32(180, 185, 190, 230); // 琴键弹起填充
static constexpr ImU32 kKeyUpHi       = IM_COL32(240, 242, 245, 200); // 琴键弹起高光
static constexpr ImU32 kKeyUpShadow   = IM_COL32(60, 65, 70, 150);   // 琴键弹起阴影
static constexpr ImU32 kKeyDownFill   = IM_COL32(90, 95, 100, 230);   // 琴键按下填充
static constexpr ImU32 kKeyDownHi     = IM_COL32(140, 145, 150, 120); // 琴键按下高光
static constexpr ImU32 kKeyDownShadow = IM_COL32(50, 55, 60, 180);   // 琴键按下阴影

static constexpr ImU32 kDecoLine      = IM_COL32(80, 85, 90, 60);     // 装饰线
static constexpr ImU32 kDecoText      = IM_COL32(90, 95, 100, 140);   // 装饰文字
static constexpr ImU32 kLabelText     = IM_COL32(60, 65, 70, 150);    // 碟面文字

static constexpr int   kKeyCount = 16;

// ============================================================
// 辅助：绘制一个径向琴键（扇形梯形段）
// ============================================================

static void DrawRadialKey(ImDrawList* draw, float cx, float cy,
                          float innerR, float outerR,
                          float startAngle, float endAngle,
                          bool isPressed)
{
    // 按下时整体向内缩 2px
    float shrink = isPressed ? 2.0f : 0.0f;
    float iR = innerR + shrink;
    float oR = outerR - shrink;

    // 用弧段近似梯形（6段弧）
    constexpr int arcSegs = 6;
    ImVec2 outer[arcSegs + 1];
    ImVec2 inner[arcSegs + 1];

    for (int s = 0; s <= arcSegs; ++s) {
        float t = startAngle + (endAngle - startAngle) * ((float)s / arcSegs);
        outer[s] = ImVec2(cx + cosf(t) * oR, cy + sinf(t) * oR);
        inner[s] = ImVec2(cx + cosf(t) * iR, cy + sinf(t) * iR);
    }

    ImU32 fillColor = isPressed ? kKeyDownFill : kKeyUpFill;

    // 填充
    for (int s = 0; s < arcSegs; ++s) {
        draw->AddTriangleFilled(inner[s], outer[s], outer[s + 1], fillColor);
        draw->AddTriangleFilled(inner[s], outer[s + 1], inner[s + 1], fillColor);
    }

    // 外弧边线（弹起=高光，按下=阴影）
    ImU32 outerEdge = isPressed ? kKeyDownShadow : kKeyUpHi;
    for (int s = 0; s < arcSegs; ++s) {
        draw->AddLine(outer[s], outer[s + 1], outerEdge, 1.5f);
    }

    // 内弧边线（弹起=阴影，按下=微亮）
    ImU32 innerEdge = isPressed ? kKeyDownHi : kKeyUpShadow;
    for (int s = 0; s < arcSegs; ++s) {
        draw->AddLine(inner[s], inner[s + 1], innerEdge, 1.0f);
    }

    // 两侧边线
    ImU32 sideColor = IM_COL32(100, 105, 110, 100);
    draw->AddLine(inner[0], outer[0], sideColor, 1.0f);
    draw->AddLine(inner[arcSegs], outer[arcSegs], sideColor, 1.0f);
}

// ============================================================
// 辅助：绘制方位箭头标记
// ============================================================

static void DrawArrowMark(ImDrawList* draw, float cx, float cy, float r,
                          float angle, float size)
{
    float ax = cx + cosf(angle) * r;
    float ay = cy + sinf(angle) * r;
    float perpAngle = angle + UITheme::kPI * 0.5f;

    ImVec2 tip(cx + cosf(angle) * (r + size), cy + sinf(angle) * (r + size));
    ImVec2 left(ax + cosf(perpAngle) * size * 0.4f, ay + sinf(perpAngle) * size * 0.4f);
    ImVec2 right(ax - cosf(perpAngle) * size * 0.4f, ay - sinf(perpAngle) * size * 0.4f);

    draw->AddTriangleFilled(tip, left, right, kDecoLine);
}

// ============================================================
// RenderTitleScreen — 启动标题画面
// ============================================================

void RenderTitleScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 vpPos  = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;

    float cx = vpPos.x + vpSize.x * 0.5f;
    float cy = vpPos.y + vpSize.y * 0.5f;

    ImDrawList* draw = ImGui::GetForegroundDrawList();

    // 自适应尺寸基准
    float baseSize = (vpSize.x < vpSize.y) ? vpSize.x : vpSize.y;
    float discR     = baseSize * 0.22f;    // 碟片半径
    float keyInnerR = discR + 15.0f;       // 琴键内半径
    float keyOuterR = discR + 70.0f;       // 琴键外半径
    float outerDecoR = keyOuterR + 14.0f;  // 装饰外圈

    // ── 背景 ──
    draw->AddRectFilled(vpPos, ImVec2(vpPos.x + vpSize.x, vpPos.y + vpSize.y), kBgColor);

    // ============================================================
    // 中央碟片
    // ============================================================

    // 碟片主体
    draw->AddCircleFilled(ImVec2(cx, cy), discR, kDiscColor, 96);

    // 碟片外边缘高光
    draw->AddCircle(ImVec2(cx, cy), discR, kDiscEdgeGlow, 96, 2.0f);

    // 外环槽线
    draw->AddCircle(ImVec2(cx, cy), discR * 0.88f, kDiscGroove, 64, 1.5f);

    // 中环槽线
    draw->AddCircle(ImVec2(cx, cy), discR * 0.65f, kDiscGroove, 48, 1.0f);

    // 内部刻度线（60条，微旋转动画）
    float rotOffset = ui.globalTime * 0.3f;
    for (int i = 0; i < 60; ++i) {
        float angle = rotOffset + (float)i * (2.0f * UITheme::kPI / 60.0f);
        float r1 = discR * 0.30f;
        float r2 = discR * 0.82f;
        ImVec2 p1(cx + cosf(angle) * r1, cy + sinf(angle) * r1);
        ImVec2 p2(cx + cosf(angle) * r2, cy + sinf(angle) * r2);
        draw->AddLine(p1, p2, kDiscTick, 1.0f);
    }

    // 内环槽线（覆盖在刻度线上方）
    draw->AddCircle(ImVec2(cx, cy), discR * 0.30f, kDiscGroove, 40, 1.0f);

    // 中心hub（深色圆）
    draw->AddCircleFilled(ImVec2(cx, cy), discR * 0.28f, kHubColor, 48);

    // hub 边缘线
    draw->AddCircle(ImVec2(cx, cy), discR * 0.28f, kDiscGroove, 48, 1.0f);

    // 中心亮点
    draw->AddCircleFilled(ImVec2(cx, cy), discR * 0.05f, kCenterDot, 24);

    // hub 内小指针线（像唱针痕迹）
    {
        float needleAngle = ui.globalTime * 0.15f;
        ImVec2 np1(cx + cosf(needleAngle) * discR * 0.06f,
                   cy + sinf(needleAngle) * discR * 0.06f);
        ImVec2 np2(cx + cosf(needleAngle) * discR * 0.26f,
                   cy + sinf(needleAngle) * discR * 0.26f);
        draw->AddLine(np1, np2, IM_COL32(200, 205, 210, 120), 1.0f);
    }

    // 碟面文字 "NEUROMANCER"
    {
        ImFont* smallFont = UITheme::GetFont_Small();
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(
            ImVec2(cx - discR * 0.58f, cy - discR * 0.22f),
            kLabelText, "NEUROMANCER");
        if (smallFont) ImGui::PopFont();
    }

    // ============================================================
    // 径向琴键环
    // ============================================================

    int activeKey = ((int)(ui.globalTime / 1.8f)) % kKeyCount;
    float slotAngle = 2.0f * UITheme::kPI / (float)kKeyCount;   // 22.5°
    float gapHalf   = slotAngle * 0.055f;               // 每侧 ~1.25°
    float startOffset = -UITheme::kPI * 0.5f;                     // 从正上方开始

    for (int i = 0; i < kKeyCount; ++i) {
        float a1 = startOffset + i * slotAngle + gapHalf;
        float a2 = startOffset + (i + 1) * slotAngle - gapHalf;
        bool pressed = (i == activeKey);
        DrawRadialKey(draw, cx, cy, keyInnerR, keyOuterR, a1, a2, pressed);
    }

    // ============================================================
    // 外圈装饰线
    // ============================================================

    draw->AddCircle(ImVec2(cx, cy), outerDecoR, kDecoLine, 64, 1.0f);

    // 四方位箭头标记（上、右、下、左）
    DrawArrowMark(draw, cx, cy, outerDecoR, -UITheme::kPI * 0.5f, 8.0f);  // 上
    DrawArrowMark(draw, cx, cy, outerDecoR, 0.0f, 8.0f);          // 右
    DrawArrowMark(draw, cx, cy, outerDecoR, UITheme::kPI * 0.5f, 8.0f);    // 下
    DrawArrowMark(draw, cx, cy, outerDecoR, UITheme::kPI, 8.0f);            // 左

    // ============================================================
    // 装饰面板（四角）
    // ============================================================

    ImFont* smallFont = UITheme::GetFont_Small();
    ImFont* termFont  = UITheme::GetFont_Terminal();

    // ── 左上角：编号 + 标签 ──
    {
        float lx = vpPos.x + 30.0f;
        float ly = vpPos.y + 30.0f;

        if (termFont) ImGui::PushFont(termFont);
        draw->AddText(ImVec2(lx, ly), kDecoText, "04");
        if (termFont) ImGui::PopFont();

        if (smallFont) ImGui::PushFont(smallFont);
        // 竖排短文字
        const char* vertTexts[] = {"C","I","R","C","L","E","S"};
        for (int i = 0; i < 7; ++i) {
            draw->AddText(ImVec2(lx + 2.0f, ly + 28.0f + i * 12.0f),
                IM_COL32(70, 75, 80, 100), vertTexts[i]);
        }

        // 小括号装饰
        draw->AddLine(ImVec2(lx - 4.0f, ly), ImVec2(lx - 4.0f, ly + 20.0f),
            kDecoLine, 1.0f);
        draw->AddLine(ImVec2(lx - 4.0f, ly), ImVec2(lx, ly),
            kDecoLine, 1.0f);
        if (smallFont) ImGui::PopFont();
    }

    // ── 右上角：数据面板 ──
    {
        float rx = vpPos.x + vpSize.x - 140.0f;
        float ry = vpPos.y + 30.0f;

        // 面板框
        draw->AddRect(ImVec2(rx, ry), ImVec2(rx + 110.0f, ry + 70.0f),
            kDecoLine, 2.0f, 0, 1.0f);

        // 数据条
        for (int i = 0; i < 3; ++i) {
            float barY = ry + 12.0f + i * 16.0f;
            float barW = 60.0f + (i % 2) * 20.0f;
            draw->AddRectFilled(
                ImVec2(rx + 8.0f, barY),
                ImVec2(rx + 8.0f + barW, barY + 6.0f),
                IM_COL32(90, 95, 100, 80), 1.0f);
        }

        // 右上小标签
        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(ImVec2(rx + 8.0f, ry + 55.0f), kDecoText, "DATA.SYS");
        if (smallFont) ImGui::PopFont();

        // 点阵装饰（4x4）
        float dotX = vpPos.x + vpSize.x - 24.0f;
        float dotY = vpPos.y + 34.0f;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                draw->AddCircleFilled(
                    ImVec2(dotX - c * 6.0f, dotY + r * 6.0f),
                    1.5f, IM_COL32(80, 85, 90, 80), 6);
            }
        }
    }

    // ── 左下角：竖排艺术文字 ──
    {
        float lx = vpPos.x + 30.0f;
        float ly = vpPos.y + vpSize.y - 160.0f;

        if (smallFont) ImGui::PushFont(smallFont);

        // 序号
        draw->AddText(ImVec2(lx, ly), IM_COL32(60, 65, 70, 80), "01  02  03  04  05");

        // 竖排 "AESTHETIC" + "MISTAKES"
        const char* word1[] = {"A","E","S","T","H","E","T","I","C"};
        const char* word2[] = {"M","I","S","T","A","K","E","S"};
        for (int i = 0; i < 9; ++i) {
            draw->AddText(ImVec2(lx, ly + 18.0f + i * 12.0f),
                IM_COL32(70, 75, 80, 100), word1[i]);
        }
        for (int i = 0; i < 8; ++i) {
            draw->AddText(ImVec2(lx + 14.0f, ly + 18.0f + i * 12.0f),
                IM_COL32(70, 75, 80, 100), word2[i]);
        }

        if (smallFont) ImGui::PopFont();
    }

    // ── 右下角：标识区 ──
    {
        float rx = vpPos.x + vpSize.x - 140.0f;
        float ry = vpPos.y + vpSize.y - 80.0f;

        // 标识框
        draw->AddRect(ImVec2(rx, ry), ImVec2(rx + 50.0f, ry + 30.0f),
            kDecoLine, 2.0f, 0, 1.0f);

        if (termFont) ImGui::PushFont(termFont);
        draw->AddText(ImVec2(rx + 10.0f, ry + 6.0f), kDecoText, "[N]");
        if (termFont) ImGui::PopFont();

        if (smallFont) ImGui::PushFont(smallFont);
        draw->AddText(ImVec2(rx + 56.0f, ry + 4.0f), kDecoText, "DESIGN AND");
        draw->AddText(ImVec2(rx + 56.0f, ry + 16.0f), kDecoText, "PRODUCTION");
        if (smallFont) ImGui::PopFont();
    }

    // ============================================================
    // 底部提示文字
    // ============================================================

    {
        if (termFont) ImGui::PushFont(termFont);

        const char* prompt = ">> PRESS ANY KEY <<";
        ImVec2 promptSize = ImGui::CalcTextSize(prompt);

        float promptY = cy + keyOuterR + 80.0f;
        // 确保不超出屏幕底部
        if (promptY + 30.0f > vpPos.y + vpSize.y - 30.0f) {
            promptY = vpPos.y + vpSize.y - 60.0f;
        }

        // 闪烁
        float blink = (sinf(ui.globalTime * UITheme::kPI * 1.5f) + 1.0f) * 0.5f;
        uint8_t alpha = (uint8_t)(100.0f + 155.0f * blink);

        draw->AddText(
            ImVec2(cx - promptSize.x * 0.5f, promptY),
            IM_COL32(190, 195, 200, alpha), prompt);

        if (termFont) ImGui::PopFont();

        // 分隔线
        float lineW = vpSize.x * 0.3f;
        draw->AddLine(
            ImVec2(cx - lineW * 0.5f, promptY + 28.0f),
            ImVec2(cx + lineW * 0.5f, promptY + 28.0f),
            kDecoLine, 1.0f);
    }

    // ============================================================
    // 底部版权信息
    // ============================================================

    {
        if (smallFont) ImGui::PushFont(smallFont);

        const char* footer = "TEAM 08 // CSC8507 // 2025";
        ImVec2 footerSize = ImGui::CalcTextSize(footer);
        draw->AddText(
            ImVec2(cx - footerSize.x * 0.5f, vpPos.y + vpSize.y - 24.0f),
            IM_COL32(50, 55, 60, 100), footer);

        if (smallFont) ImGui::PopFont();
    }

    // ============================================================
    // 过渡逻辑：任意键/鼠标 → Splash
    // ============================================================

    if (ui.titleTimer > 0.5f) {
        bool anyInput = false;

        const Keyboard* kb = Window::GetKeyboard();
        if (kb) {
            for (int k = 0; k < KeyCodes::MAXVALUE; ++k) {
                if (kb->KeyPressed(static_cast<KeyCodes::Type>(k)) && k >= KeyCodes::BACK) {
                    anyInput = true;
                    break;
                }
            }
        }

        if (!anyInput) {
            const Mouse* mouse = Window::GetMouse();
            if (mouse && (mouse->ButtonPressed(NCL::MouseButtons::Left) ||
                          mouse->ButtonPressed(NCL::MouseButtons::Right))) {
                anyInput = true;
            }
        }

        if (anyInput) {
            ui.activeScreen = UIScreen::Splash;
            ui.splashTimer = 0.0f;
            LOG_INFO("[UI_TitleScreen] TitleScreen -> Splash");
            return;
        }
    }
}

} // namespace ECS::UI

#endif // USE_IMGUI
