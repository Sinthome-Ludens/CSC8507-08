#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief PhysicsTest 场景敌人生成控制 ImGui 面板系统
 *
 * 职责：
 *   - 渲染敌人生成/删除控制面板（依赖 Res_EnemyTestState context）
 *   - 通过 PrefabFactory::CreatePhysicsEnemy() 生成敌人实体
 *   - 删除最后生成的敌人实体
 *
 * 场景专属：依赖 Res_EnemyTestState，仅用于 PhysicsTest 场景。
 * 系统无状态，所有数据存储于 Res_EnemyTestState context。
 *
 * 执行优先级：320（EnemyMonitor 310 之后）
 */
class Sys_ImGuiPhysicsTest : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    void RenderPhysicsTestWindow(Registry& registry);
    void SpawnEnemy             (Registry& registry);
    void DeleteLastEnemy        (Registry& registry);

    bool m_ShowWindow = true;
};

} // namespace ECS
#endif
