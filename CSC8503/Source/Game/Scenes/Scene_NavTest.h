#pragma once

#include "IScene.h"
#include "Game/Utils/NavMeshPathfinderUtil.h"
#include <memory>

/**
 * @brief 导航测试场景
 *
 * 注册并启动以下系统：
 *   - Sys_Camera      (50)  — 自由相机实体 + NCL Bridge
 *   - Sys_Physics     (100) — Jolt 物理引擎（Body 创建 + Transform 同步）
 *   - Sys_Navigation  (150) — NavAgent 路径查找 + 物理速度驱动移动
 *   - Sys_Render      (200) — ECS 实体 → NCL 代理 GameObject 桥接渲染
 *   - Sys_EnemyAI     (250) — 敌人警戒状态机
 *   - Sys_ImGui       (300) — 菜单栏 + Debug 窗口    [仅 USE_IMGUI]
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

private:
    /// 寻路器实例（生命周期必须覆盖 Sys_Navigation，存于此处确保 OnExit 后才释放）
    std::unique_ptr<ECS::NavMeshPathfinderUtil> m_Pathfinder;
};
