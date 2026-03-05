#pragma once

#include "IScene.h"

/**
 * @brief 物理测试场景（ECS 架构验证场景）
 *
 * 注册并启动以下系统：
 *   - Sys_Camera   (50)  — 自由相机实体 + NCL Bridge
 *   - Sys_Physics  (100) — Jolt 物理引擎（Body 创建 + Transform 同步）
 *   - Sys_Render   (200) — ECS 实体 → NCL 代理 GameObject 桥接渲染
 *   - Sys_ImGui    (300) — 菜单栏 + Debug 窗口        [仅 USE_IMGUI]
 *   - Sys_TestScene(400) — 方块工厂 + 交互控制面板    [仅 USE_IMGUI]
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
};
