#pragma once

#include "IScene.h"
#include "Game/Utils/NavMeshPathfinderUtil.h"
#include <memory>

/**
 * @brief 导航测试场景
 *
 * 注册并启动以下系统：
 *   - Sys_Camera         (50)  — 自由相机实体 + NCL Bridge
 *   - Sys_Physics        (100) — Jolt 物理引擎（Body 创建 + Transform 同步）
 *   - Sys_EnemyVision    (110) — 敌人视野判定（扇形视锥 + 遮挡射线）
 *   - Sys_DeathJudgment  (125) — 死亡判定（敌人抓捕 + HP归零 + 触发器即死）
 *   - Sys_DeathEffect    (126) — 赛博朋克四阶段死亡动画，动画结束后销毁实体
 *   - Sys_Navigation     (130) — NavAgent 路径查找 + 物理速度驱动移动
 *   - Sys_Render         (200) — ECS 实体 → NCL 代理 GameObject 桥接渲染
 *   - Sys_EnemyAI        (250) — 敌人警戒状态机（读取 C_D_AIPerception::is_spotted）
 *   - Sys_ImGui          (300) — 菜单栏 + Debug 窗口    [仅 USE_IMGUI]
 *   - Sys_ImGuiNavTest   (310) — NavTest 敌人/目标生成控制面板 [仅 USE_IMGUI]
 */
class Scene_NavTest : public IScene {
public:
    Scene_NavTest()  = default;
    ~Scene_NavTest() = default;

    void OnEnter(ECS::Registry&          registry,
                 ECS::SystemManager&     systems,
                 const Res_NCL_Pointers& nclPtrs) override;

    void OnExit(ECS::Registry&      registry,
                ECS::SystemManager& systems) override;

    IScene* CreateRestartScene() override { return new Scene_NavTest(); }

private:
    /// 寻路器实例（生命周期必须覆盖 Sys_Navigation，存于此处确保 OnExit 后才释放）
    std::unique_ptr<ECS::NavMeshPathfinderUtil> m_Pathfinder;
};
