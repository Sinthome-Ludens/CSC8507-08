#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <vector>

/**
 * @brief NavTest 场景全量状态（全局资源 / Registry Context）
 *
 * 由 Scene_NavTest::OnEnter() 初始化并注册到 Registry context。
 * 由 Sys_ImGuiNavTest 在每帧读写（SpawnEnemy_Nav / SpawnTarget / Delete*）。
 *
 * 注意：作为 Res_* 全局资源（非 Component），可合法使用 std::vector。
 */
struct Res_NavTestState {
    ECS::MeshHandle            enemyMeshHandle  = ECS::INVALID_HANDLE; ///< 共享 enemy mesh 句柄
    std::vector<ECS::EntityID> enemyEntities;                          ///< 存活的导航敌人实体列表
    int                        enemySpawnIndex  = 0;                   ///< 下一个 enemy 的生成索引

    ECS::MeshHandle            targetMeshHandle = ECS::INVALID_HANDLE; ///< 共享 target mesh 句柄
    std::vector<ECS::EntityID> targetEntities;                         ///< 存活的导航目标实体列表
    int                        targetSpawnIndex = 0;                   ///< 下一个 target 的生成索引
};
