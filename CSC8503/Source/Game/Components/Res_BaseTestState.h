#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include <vector>

/**
 * @brief 通用测试场景状态（全局资源 / Registry Context）
 *
 * 所有测试场景均可注册此 context，用于支持基础的 Cube/Capsule 生成功能。
 * 由各 Scene::OnEnter() 初始化并注册，由 Sys_ImGui 的 RenderBaseTestWindow 读写。
 *
 * 注意：作为 Res_* 全局资源（非 Component），可合法使用 std::vector。
 */
struct Res_BaseTestState {
    ECS::MeshHandle            cubeMeshHandle    = ECS::INVALID_HANDLE; ///< 共享 cube mesh 句柄
    std::vector<ECS::EntityID> cubeEntities;                            ///< 存活的动态 cube 实体列表
    int                        spawnIndex        = 0;                   ///< 下一个 cube 的生成偏移索引

    ECS::MeshHandle            capsuleMeshHandle = ECS::INVALID_HANDLE; ///< 共享 capsule mesh 句柄
    std::vector<ECS::EntityID> capsuleEntities;                         ///< 存活的动态 capsule 实体列表
    int                        capsuleSpawnIndex = 0;                   ///< 下一个 capsule 的生成偏移索引
};
