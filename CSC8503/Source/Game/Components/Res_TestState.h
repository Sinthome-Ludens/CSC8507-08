#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <vector>

/**
 * @brief PhysicsTest 场景专属调试状态（全局资源 / Registry Context）
 *
 * 由 Scene_PhysicsTest::OnEnter() 初始化并注册到 Registry context。
 * 负责 Enemy/Target 实体的生命周期管理，由 Sys_ImGui 在每帧读写。
 *
 * Cube/Capsule 通用功能已移至 Res_BaseTestState（所有场景共用）。
 *
 * 注意：作为 Res_* 全局资源（非 Component），可合法使用 std::vector。
 */
struct Res_TestState {
    ECS::MeshHandle            enemyMeshHandle  = ECS::INVALID_HANDLE; ///< 共享 enemy mesh 句柄
    std::vector<ECS::EntityID> enemyEntities;                          ///< 存活的动态 enemy 实体列表
    int                        enemySpawnIndex  = 0;                   ///< 下一个 enemy 的生成偏移索引

    ECS::MeshHandle            targetMeshHandle = ECS::INVALID_HANDLE; ///< 共享 target mesh 句柄
    std::vector<ECS::EntityID> targetEntities;                         ///< 存活的动态 target 实体列表
    int                        targetSpawnIndex = 0;                   ///< 下一个 target 的生成偏移索引
};
