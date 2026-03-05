#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <vector>

/**
 * @brief 物理测试场景调试状态（全局资源 / Engine Context）
 *
 * 由 Scene_PhysicsTest::OnEnter() 初始化并注册到 Registry context，
 * 由 Sys_ImGui 在每帧读写（Spawn / Delete / Gravity 操作）。
 *
 * 注意：作为 Res_* 全局资源（非 Component），可合法使用 std::vector。
 * Component 才受 POD 约束，全局资源不在此限。
 */
struct Res_TestState {
    ECS::MeshHandle            cubeMeshHandle = ECS::INVALID_HANDLE; ///< 共享 cube mesh 句柄
    std::vector<ECS::EntityID> cubeEntities;                         ///< 存活的动态 cube 实体列表
    int                        spawnIndex     = 0;                   ///< 下一个 cube 的生成偏移索引

    ECS::MeshHandle            capsuleMeshHandle = ECS::INVALID_HANDLE; ///< 共享 capsule mesh 句柄
    std::vector<ECS::EntityID> capsuleEntities;                         ///< 存活的动态 capsule 实体列表
    int                        capsuleSpawnIndex = 0;                   ///< 下一个 capsule 的生成偏移索引
};
