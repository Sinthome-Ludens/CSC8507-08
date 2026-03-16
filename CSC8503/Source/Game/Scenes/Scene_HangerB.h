/**
 * @file Scene_HangerB.h
 * @brief HangerB 关卡场景定义（HangerB.obj + HangerB.navmesh）。
 */
#pragma once

#include "IScene.h"
#include "Game/Utils/NavMeshPathfinderUtil.h"
#include <memory>

/**
 * @brief HangerB 关卡（完整玩法系统）
 *
 * 注册并启动以下系统：
 *   - Sys_Input           (10)  — NCL → Res_Input
 *   - Sys_InputDispatch   (55)  — Res_Input → per-entity C_D_Input
 *   - Sys_PlayerDisguise  (59)  — 伪装切换
 *   - Sys_PlayerStance    (60)  — 蹲/站切换
 *   - Sys_StealthMetrics  (62)  — 潜行指标计算
 *   - Sys_PlayerCQC       (63)  — CQC 近身制服
 *   - Sys_Movement        (65)  — 物理移动
 *   - Sys_Physics         (100) — Jolt 物理引擎
 *   - Sys_EnemyVision     (110) — 敌人视野判定
 *   - Sys_EnemyAI         (120) — 敌人状态机
 *   - Sys_DeathJudgment   (125) — 死亡判定
 *   - Sys_DeathEffect     (126) — 死亡视觉特效
 *   - Sys_LevelGoal       (127) — 关卡目标
 *   - Sys_Navigation      (130) — NavAgent 路径查找
 *   - Sys_PlayerCamera    (150) — 第三人称跟随相机
 *   - Sys_Camera          (155) — 相机 Bridge + debug 飞行
 *   - Sys_Render          (200) — ECS → NCL 桥接渲染
 *   - Sys_Item            (250) — 道具管理
 *   - Sys_ItemEffects     (260) — 道具效果执行
 *   - Sys_ImGui           (300) — 菜单栏 + Debug 窗口          [仅 USE_IMGUI]
 *   - Sys_ImGuiEntityDebug(305) — 实体调试面板                  [仅 USE_IMGUI]
 *   - Sys_ImGuiEnemyAI    (310) — 敌人状态监控                  [仅 USE_IMGUI]
 *   - Sys_ImGuiNavTest    (315) — NavTest 敌人/目标生成控制面板  [仅 USE_IMGUI]
 *   - Sys_Countdown       (350) — 警报倒计时
 *   - Sys_Chat            (450) — 对话逻辑                      [仅 USE_IMGUI]
 *   - Sys_UI              (500) — UI 渲染 + 输入导航            [仅 USE_IMGUI]
 */
class Scene_HangerB : public IScene {
public:
    Scene_HangerB()  = default;
    ~Scene_HangerB() = default;

    void OnEnter(ECS::Registry&          registry,
                 ECS::SystemManager&     systems,
                 const Res_NCL_Pointers& nclPtrs) override;

    void OnExit(ECS::Registry&      registry,
                ECS::SystemManager& systems) override;

    IScene* CreateRestartScene() override { return new Scene_HangerB(); }

private:
    /// 寻路器实例（生命周期必须覆盖 Sys_Navigation，存于此处确保 OnExit 后才释放）
    std::unique_ptr<ECS::NavMeshPathfinderUtil> m_Pathfinder;
};
