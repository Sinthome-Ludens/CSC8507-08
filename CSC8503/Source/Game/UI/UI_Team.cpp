#include "UI_Team.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <cstring>
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// 团队成员数据（按首字母排序）
// ============================================================

struct TeamMember {
    const char* name;
    const char* role;
};

static constexpr TeamMember kMembers[] = {
    {"Beining Zhang",  "AI Behavior Tree & State Machine"},
    {"Dong Yijun",     "Rendering (VFX + Camera) & AI FSM"},
    {"Penggan Zhao",   "Gameplay - UI & Scene"},
    {"Shuliang Chu",   "Behavior & Debug, Player Controller"},
    {"Weifeng Chen",   "Networking"},
    {"Yihan Lin",      "Gameplay - Item System"},
    {"Yu Jianran",     "ECS Bridge, Rendering (Lighting + PBR)"},
};
static constexpr int kMemberCount = 7;

// ============================================================
// 动画时间轴常量
// ============================================================

static constexpr float BOOT_START      = 0.0f;     // 启动序列开始
static constexpr float BOOT_DURATION   = 1.5f;     // 启动文本打字时长
static constexpr float MEMBER_START    = 1.8f;     // 第一个成员出现时间
static constexpr float MEMBER_INTERVAL = 0.75f;    // 每个成员间隔
static constexpr float MEMBER_TYPEWR   = 0.4f;     // 单条打字机持续时间
static constexpr float OK_DELAY        = 0.3f;     // [  ] → [OK] 延迟
static constexpr float PROJECT_START   = MEMBER_START + kMemberCount * MEMBER_INTERVAL + 0.5f;
static constexpr float PROJECT_DUR     = 1.2f;     // 项目标题打字机时长
static constexpr float FULL_SEQUENCE   = PROJECT_START + PROJECT_DUR + 0.5f;

// ============================================================
// 辅助：打字机效果 — 根据进度截断字符串
// ============================================================

static int TypewriterLen(const char* text, float progress) {
    if (progress >= 1.0f) return static_cast<int>(strlen(text));
    if (progress <= 0.0f) return 0;
    return static_cast<int>(strlen(text) * progress);
}

// ============================================================
// 辅助：构建带点线对齐的完整行
// ============================================================

static void BuildDottedLine(char* buf, int bufSize,
                            const char* name, const char* role,
                            int totalWidth) {
    int nameLen = static_cast<int>(strlen(name));
    int roleLen = static_cast<int>(strlen(role));
    int dotsNeeded = totalWidth - nameLen - roleLen;
    if (dotsNeeded < 3) dotsNeeded = 3;

    int pos = 0;
    // name
    for (int i = 0; i < nameLen && pos < bufSize - 1; ++i)
        buf[pos++] = name[i];
    // space
    if (pos < bufSize - 1) buf[pos++] = ' ';
    // dots
    for (int i = 0; i < dotsNeeded - 2 && pos < bufSize - 1; ++i)
        buf[pos++] = '.';
    // space
    if (pos < bufSize - 1) buf[pos++] = ' ';
    // role
    for (int i = 0; i < roleLen && pos < bufSize - 1; ++i)
        buf[pos++] = role[i];

    buf[pos] = '\0';
}

// ============================================================
// 辅助：EaseOutCubic
// ============================================================

static float EaseOutCubic(float t) {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}

// ============================================================
// RenderTeamScreen — 主渲染函数
// ============================================================

void RenderTeamScreen(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    auto& ui = registry.ctx<Res_UIState>();

    // 计算自进入画面以来的 elapsed 时间
    float elapsed = ui.globalTime - ui.teamStartTime;
    if (elapsed < 0.0f) elapsed = 0.0f;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##Team", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float vpW = vp->Size.x;
    const float vpH = vp->Size.y;

    // 全屏深色背景
    draw->AddRectFilled(vp->Pos,
                        ImVec2(vp->Pos.x + vpW, vp->Pos.y + vpH),
                        IM_COL32(4, 6, 10, 250));

    // ── 布局参数 ──
    constexpr float CONTENT_W  = 580.0f;
    const float contentX = vp->Pos.x + (vpW - CONTENT_W) * 0.5f;
    float curY = vp->Pos.y + vpH * 0.12f;
    constexpr float LINE_H = 28.0f;

    // ── Phase 1: 终端启动文本 ──
    {
        ImFont* termFont = UITheme::GetFont_Terminal();
        if (termFont) ImGui::PushFont(termFont);

        const char* bootText = "> INITIALIZING TEAM MANIFEST...";

        if (elapsed >= BOOT_START) {
            float bootProgress = (elapsed - BOOT_START) / BOOT_DURATION;
            int showLen = TypewriterLen(bootText, bootProgress);

            // 绘制已显示部分
            char bootBuf[64] = {};
            if (showLen > 0) {
                int copyLen = (showLen < 63) ? showLen : 63;
                for (int i = 0; i < copyLen; ++i) bootBuf[i] = bootText[i];
                bootBuf[copyLen] = '\0';
                draw->AddText(ImVec2(contentX, curY),
                              IM_COL32(0, 200, 190, 230), bootBuf);
            }

            // 闪烁光标 _
            if (bootProgress < 1.0f) {
                float cursorAlpha = (sinf(ui.globalTime * UITheme::kPI * 4.0f) > 0.0f) ? 200.0f : 0.0f;
                ImVec2 partSize = ImGui::CalcTextSize(bootBuf);
                draw->AddText(ImVec2(contentX + partSize.x, curY),
                              IM_COL32(0, 220, 210, static_cast<uint8_t>(cursorAlpha)), "_");
            }

            // 启动完成后追加 [DONE]
            if (bootProgress >= 1.0f) {
                ImVec2 fullSize = ImGui::CalcTextSize(bootText);
                float doneAlpha = (elapsed - BOOT_START - BOOT_DURATION) * 4.0f;
                if (doneAlpha > 1.0f) doneAlpha = 1.0f;
                draw->AddText(ImVec2(contentX + fullSize.x + 8.0f, curY),
                              IM_COL32(76, 204, 76, static_cast<uint8_t>(doneAlpha * 220)),
                              "[DONE]");
            }
        }

        if (termFont) ImGui::PopFont();
        curY += LINE_H + 4.0f;
    }

    // ── 分隔线 ──
    if (elapsed > BOOT_DURATION * 0.8f) {
        float lineAlpha = (elapsed - BOOT_DURATION * 0.8f) * 3.0f;
        if (lineAlpha > 1.0f) lineAlpha = 1.0f;
        draw->AddLine(ImVec2(contentX, curY),
                      ImVec2(contentX + CONTENT_W, curY),
                      IM_COL32(0, 140, 130, static_cast<uint8_t>(lineAlpha * 80)), 1.0f);
    }
    curY += 12.0f;

    // ── Phase 2: 成员逐行揭示 ──
    {
        ImFont* termFont = UITheme::GetFont_Terminal();
        if (termFont) ImGui::PushFont(termFont);

        for (int i = 0; i < kMemberCount; ++i) {
            float memberTime = MEMBER_START + i * MEMBER_INTERVAL;
            if (elapsed < memberTime) {
                curY += LINE_H;
                continue;
            }

            float age = elapsed - memberTime;

            // 滑入效果
            float slideT = age / 0.2f;
            float slideOffset = (1.0f - EaseOutCubic(slideT)) * 100.0f;

            // 整行 alpha
            float rowAlpha = age / 0.15f;
            if (rowAlpha > 1.0f) rowAlpha = 1.0f;

            // 构建带点线的完整行
            char lineBuf[128];
            BuildDottedLine(lineBuf, sizeof(lineBuf),
                            kMembers[i].name, kMembers[i].role, 56);

            // 打字机截断
            float typeProgress = age / MEMBER_TYPEWR;
            int showLen = TypewriterLen(lineBuf, typeProgress);

            char displayBuf[128] = {};
            if (showLen > 0) {
                int copyLen = (showLen < 127) ? showLen : 127;
                for (int c = 0; c < copyLen; ++c) displayBuf[c] = lineBuf[c];
                displayBuf[copyLen] = '\0';
            }

            float drawX = contentX + 40.0f - slideOffset;
            uint8_t alpha = static_cast<uint8_t>(rowAlpha * 230);

            // [  ] 或 [OK] 状态标签
            if (age < OK_DELAY) {
                draw->AddText(ImVec2(drawX - 36.0f, curY),
                              IM_COL32(100, 110, 120, alpha), "[  ]");
            } else {
                // [OK] 闪一下亮绿色再恢复青色
                float okAge = age - OK_DELAY;
                uint8_t gVal = (okAge < 0.15f) ? 255 : 204;
                uint8_t rVal = (okAge < 0.15f) ? 150 : 0;
                draw->AddText(ImVec2(drawX - 36.0f, curY),
                              IM_COL32(rVal, gVal, rVal / 2, alpha), "[OK]");
            }

            // 成员行文本
            draw->AddText(ImVec2(drawX, curY),
                          IM_COL32(200, 215, 220, alpha), displayBuf);

            // 打字机光标
            if (typeProgress < 1.0f && typeProgress > 0.0f) {
                float cursorBlink = (sinf(ui.globalTime * UITheme::kPI * 5.0f) > 0.0f) ? 1.0f : 0.0f;
                ImVec2 partSize = ImGui::CalcTextSize(displayBuf);
                draw->AddText(ImVec2(drawX + partSize.x, curY),
                              IM_COL32(0, 220, 210, static_cast<uint8_t>(cursorBlink * alpha)), "_");
            }

            curY += LINE_H;
        }

        if (termFont) ImGui::PopFont();
    }

    curY += 16.0f;

    // ── Phase 3: 项目标题 ──
    if (elapsed >= PROJECT_START) {
        float projAge = elapsed - PROJECT_START;

        // 分隔线（双线）
        float sepAlpha = projAge * 3.0f;
        if (sepAlpha > 1.0f) sepAlpha = 1.0f;
        ImU32 sepColor = IM_COL32(0, 180, 170, static_cast<uint8_t>(sepAlpha * 120));
        draw->AddLine(ImVec2(contentX + 80.0f, curY), ImVec2(contentX + CONTENT_W - 80.0f, curY), sepColor, 2.0f);
        curY += 16.0f;

        ImFont* termFont = UITheme::GetFont_Terminal();
        if (termFont) ImGui::PushFont(termFont);

        // 项目名
        {
            const char* projLine = "PROJECT NEUROMANCER  //  CSC8507";
            float projProg = projAge / PROJECT_DUR;
            int showLen = TypewriterLen(projLine, projProg);
            char projBuf[64] = {};
            if (showLen > 0) {
                int copyLen = (showLen < 63) ? showLen : 63;
                for (int c = 0; c < copyLen; ++c) projBuf[c] = projLine[c];
                projBuf[copyLen] = '\0';
            }
            float projX = contentX + (CONTENT_W - ImGui::CalcTextSize(projLine).x) * 0.5f;

            // 发光底层
            uint8_t glowA = static_cast<uint8_t>(sepAlpha * 60);
            draw->AddText(ImVec2(projX, curY), IM_COL32(0, 110, 105, glowA), projBuf);
            draw->AddText(ImVec2(projX, curY), IM_COL32(0, 220, 210, static_cast<uint8_t>(sepAlpha * 240)), projBuf);
            curY += LINE_H;
        }

        // 大学+年份
        {
            const char* uniLine = "Newcastle University  //  2026";
            float uniProg = (projAge - 0.4f) / PROJECT_DUR;
            if (uniProg < 0.0f) uniProg = 0.0f;
            int showLen = TypewriterLen(uniLine, uniProg);
            char uniBuf[64] = {};
            if (showLen > 0) {
                int copyLen = (showLen < 63) ? showLen : 63;
                for (int c = 0; c < copyLen; ++c) uniBuf[c] = uniLine[c];
                uniBuf[copyLen] = '\0';
            }
            float uniX = contentX + (CONTENT_W - ImGui::CalcTextSize(uniLine).x) * 0.5f;
            draw->AddText(ImVec2(uniX, curY), IM_COL32(130, 140, 150, static_cast<uint8_t>(sepAlpha * 200)), uniBuf);
            curY += LINE_H;
        }

        // 底部分隔线
        curY += 4.0f;
        draw->AddLine(ImVec2(contentX + 80.0f, curY), ImVec2(contentX + CONTENT_W - 80.0f, curY), sepColor, 2.0f);

        if (termFont) ImGui::PopFont();
    }

    // ── Phase 4: 底部提示 ──
    if (elapsed >= FULL_SEQUENCE) {
        ImFont* smallFont = UITheme::GetFont_Small();
        if (smallFont) ImGui::PushFont(smallFont);

        const char* prompt = ">> PRESS ESC TO RETURN <<";
        ImVec2 promptSize = ImGui::CalcTextSize(prompt);
        float promptX = vp->Pos.x + (vpW - promptSize.x) * 0.5f;
        float promptY = vp->Pos.y + vpH * 0.88f;

        float blink = sinf(ui.globalTime * UITheme::kPI * 1.5f);
        uint8_t promptAlpha = static_cast<uint8_t>((blink * 0.5f + 0.5f) * 200.0f);
        draw->AddText(ImVec2(promptX, promptY),
                      IM_COL32(0, 200, 190, promptAlpha), prompt);

        if (smallFont) ImGui::PopFont();
    }

    // ── 水平扫描线装饰（缓慢下移）──
    {
        float scanY = vp->Pos.y + fmodf(elapsed * 40.0f, vpH);
        draw->AddLine(ImVec2(vp->Pos.x, scanY),
                      ImVec2(vp->Pos.x + vpW, scanY),
                      IM_COL32(0, 200, 190, 15), 1.0f);
        // 第二条扫描线（半屏偏移）
        float scanY2 = vp->Pos.y + fmodf(elapsed * 40.0f + vpH * 0.5f, vpH);
        draw->AddLine(ImVec2(vp->Pos.x, scanY2),
                      ImVec2(vp->Pos.x + vpW, scanY2),
                      IM_COL32(0, 200, 190, 10), 1.0f);
    }

    ImGui::End();
}

} // namespace ECS::UI

#endif // USE_IMGUI
