#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <vector>

/**
 * @brief 胶囊生成调试状态（全局资源 / Registry Context）
 *
 * 由 Scene_PhysicsTest::OnEnter() 初始化并注册到 Registry context。
 * 由 Sys_ImGuiCapsuleGen 在每帧读写（SpawnCapsule / DeleteLastCapsule）。
 *
 * 注意：作为 Res_* 全局资源（非 Component），可合法使用 std::vector。
 */
struct Res_CapsuleState {
    ECS::MeshHandle            capsuleMeshHandle = ECS::INVALID_HANDLE; ///< 共享 capsule mesh 句柄
    std::vector<ECS::EntityID> capsuleEntities;                         ///< 存活的动态 capsule 实体列表
    int                        capsuleSpawnIndex = 0;                   ///< 下一个 capsule 的生成偏移索引
};
