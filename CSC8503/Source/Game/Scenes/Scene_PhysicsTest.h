#pragma once

#include "IScene.h"

/**
 * @brief 物理测试场景（ECS 架构验证场景）
 *
 * 注册并启动以下系统（优先级升序）：
 *   - Sys_Input           ( 10) — NCL 输入 → Res_Input（via InputAdapter）
 *   - Sys_InputDispatch   ( 55) — Res_Input → per-entity C_D_Input
 *   - Sys_PlayerDisguise  ( 59) — 伪装切换、C_T_Hidden 管理
 *   - Sys_PlayerStance    ( 60) — 蹲/站切换、碰撞体替换
 *   - Sys_StealthMetrics  ( 62) — 奔跑、速度乘数、噪音、可见度
 *   - Sys_PlayerCQC       ( 63) — CQC 近身制服 + 目标选择
 *   - Sys_Movement        ( 65) — 物理移动
 *   - Sys_Physics         (100) — Jolt Body 创建 + 物理步进 + Transform 同步
 *   - Sys_EnemyVision     (110) — 敌人视野判定（扇形视锥 + 遮挡射线）
 *   - Sys_EnemyAI         (120) — 敌人感知检测 + 四状态切换（Safe/Search/Alert/Hunt）
 *   - Sys_DeathJudgment   (125) — 死亡判定（敌人抓捕 + HP归零 + 触发器即死）
 *   - Sys_DeathEffect     (126) — 赛博朋克四阶段死亡动画，动画结束后销毁实体
 *   - Sys_PlayerCamera    (150) — 第三人称跟随相机
 *   - Sys_Camera          (155) — 相机实体创建 + NCL Bridge 同步 + debug 飞行
 *   - Sys_Render          (200) — ECS 实体 → NCL 代理 GameObject 桥接渲染
 *   - Sys_ImGui           (300) — 菜单栏 + 性能窗口 + Cube/Capsule 控制面板  [仅 USE_IMGUI]
 *   - Sys_ImGuiEnemyAI    (310) — 通用敌人状态监控表格（场景无关）          [仅 USE_IMGUI]
 *   - Sys_ImGuiPhysicsTest(320) — PhysicsTest 场景敌人生成/删除控制面板      [仅 USE_IMGUI]
 *   - Sys_Raycast         (330) — Raycast 独立测试窗口（按钮触发 + 可视化）
 *   - Sys_Countdown       (350) — alertLevel≥100 → 30s 倒计时 → GameOver
 *   - Sys_Chat            (450) — 对话逻辑                                   [仅 USE_IMGUI]
 *   - Sys_UI              (500) — UI 渲染 + 输入导航                         [仅 USE_IMGUI]
 *
 * OnEnter：注册系统 → AwakeAll（包括创建相机实体、地板实体）
 * OnExit ：DestroyAll（逆序停机）→ ctx_erase（场景级资源）→ registry.Clear()
 */
class Scene_PhysicsTest : public IScene {
public:
    Scene_PhysicsTest()  = default;
    ~Scene_PhysicsTest() = default;

    void OnEnter(ECS::Registry&          registry,
                 ECS::SystemManager&     systems,
                 const Res_NCL_Pointers& nclPtrs) override;

    void OnExit(ECS::Registry&      registry,
                ECS::SystemManager& systems) override;

    IScene* CreateRestartScene() override { return new Scene_PhysicsTest(); }
};
