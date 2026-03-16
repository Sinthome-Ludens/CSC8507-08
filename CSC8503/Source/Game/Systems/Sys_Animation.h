/**
 * @file Sys_Animation.h
 * @brief 骨骼动画系统：驱动 C_D_Animation 组件播放，计算骨骼矩阵。
 *
 * @details
 * - OnAwake：从 AssetManager 预热所需的动画剪辑
 * - OnFixedUpdate：推进动画时间，处理循环/停止逻辑
 * - OnUpdate：采样 MeshAnimation 帧数据，计算 inverseBindPose × frameJoint 矩阵，
 *             写入 C_D_Animation.boneMatrices（供 Sys_Render 上传 GPU）
 *
 * 执行优先级：50（Sys_Physics = 100 之前，确保骨骼姿态先于物理更新）
 */
#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Game/Components/C_D_Animation.h"
#include "Game/Components/C_D_MeshRenderer.h"

namespace NCL::Rendering {
    class MeshAnimation;
    class OGLMesh;
}

namespace ECS {

class AssetManager;

/**
 * @brief 骨骼动画驱动系统
 *
 * 遍历 C_D_Animation + C_D_MeshRenderer 实体，按帧推进动画时间并写入骨骼矩阵。
 * 系统本身无状态，所有运行时数据存于 C_D_Animation 组件。
 */
class Sys_Animation : public ISystem {
public:
    Sys_Animation() = default;

    void OnAwake  (Registry& registry) override;
    void OnFixedUpdate(Registry& registry, float fixedDt) override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override {}

private:
    /// @brief 根据动画句柄和时间采样骨骼矩阵，写入 anim.boneMatrices
    void SampleAnimation(C_D_Animation& anim, NCL::Rendering::MeshAnimation* clip,
                         NCL::Rendering::OGLMesh* mesh);
};

} // namespace ECS
