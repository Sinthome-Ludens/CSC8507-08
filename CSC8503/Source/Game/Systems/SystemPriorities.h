/**
 * @file SystemPriorities.h
 * @brief 系统优先级命名常量——消除场景中的魔法数字，显式表达依赖关系。
 *
 * 规则：数值越小越先执行（OnUpdate/OnFixedUpdate），DestroyAll 按逆序。
 * 依赖关系通过表达式显式声明（如 OrbitTriangle = Physics + 1）。
 */
#pragma once

namespace ECS::Priority {

// ── 输入层 ──
constexpr int Input           = 10;

// ── 动画 ──
constexpr int Animation       = 50;

// ── 网络（可选，multiplayer） ──
constexpr int Network         = 54;
constexpr int InputDispatch   = 55;
constexpr int Interpolation   = 56;

// ── 玩家行为链 ──
constexpr int PlayerDisguise  = 59;
constexpr int PlayerStance    = 60;
constexpr int StealthMetrics  = 62;
constexpr int PlayerCQC       = 63;
constexpr int Movement        = 65;

// ── 装饰 ──
constexpr int Spin            = 66;

// ── 物理 + 物理后 ──
constexpr int Physics         = 100;
constexpr int OrbitTriangle   = Physics + 1;   // 必须在 Physics 同步 Transform 之后

// ── AI 链 ──
constexpr int EnemyVision     = 110;
constexpr int EnemyAI         = EnemyVision + 10;
constexpr int DeathJudgment   = EnemyAI + 5;
constexpr int DeathEffect     = DeathJudgment + 1;
constexpr int LevelGoal       = DeathEffect + 1;

// ── 导航 ──
constexpr int Navigation      = 130;

// ── 相机 ──
constexpr int PlayerCamera    = 150;
constexpr int Camera          = PlayerCamera + 5;

// ── 环境 ──
constexpr int DataOcean       = 195;

// ── 渲染 ──
constexpr int Render          = 200;

// ── 道具 ──
constexpr int Item            = 250;
constexpr int ItemEffects     = Item + 10;
constexpr int Door            = ItemEffects + 10;

// ── 音频 ──
constexpr int Audio           = 275;

// ── ImGui / Debug ──
constexpr int ImGui           = 300;
constexpr int ImGuiEntityDebug = ImGui + 5;
constexpr int ImGuiEnemyAI    = ImGui + 10;
constexpr int ImGuiNavTest    = ImGui + 15;
constexpr int ImGuiRenderDebug = ImGui + 20;

// ── 倒计时 ──
constexpr int Countdown       = 350;

// ── 对话 / UI ──
constexpr int Chat            = 450;
constexpr int UI              = 500;

} // namespace ECS::Priority
