#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <vector>

/**
 * @brief PhysicsTest 场景敌人调试状态（全局资源 / Registry Context）
 *
 * 由 Scene_PhysicsTest::OnEnter() 初始化并注册到 Registry context。
 * 由 Sys_ImGuiPhysicsTest 在每帧读写（SpawnEnemy / DeleteLastEnemy）。
 *
 * 注意：作为 Res_* 全局资源（非 Component），可合法使用 std::vector。
 */
struct Res_EnemyTestState {
    ECS::MeshHandle            enemyMeshHandle = ECS::INVALID_HANDLE; ///< 共享 enemy mesh 句柄
    std::vector<ECS::EntityID> enemyEntities;                         ///< 存活的 enemy 实体列表
    int                        enemySpawnIndex = 0;                   ///< 下一个 enemy 的生成偏移索引
};
