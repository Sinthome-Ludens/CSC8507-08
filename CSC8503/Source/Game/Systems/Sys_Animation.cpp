/**
 * @file Sys_Animation.cpp
 * @brief 骨骼动画系统实现：推进动画时间，采样骨骼矩阵，写入 C_D_Animation.boneMatrices
 */

#include "Sys_Animation.h"
#include "Game/Utils/PauseGuard.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Utils/Log.h"

#include "Mesh.h"
#include "MeshAnimation.h"

#include <cmath>
#include <algorithm>

using namespace NCL;
using namespace NCL::Rendering;
using namespace NCL::Maths;

namespace ECS {

void Sys_Animation::OnAwake(Registry& registry) {
    registry.view<C_D_Animation>().each([](EntityID id, C_D_Animation& anim) {
        if (anim.animHandle == 0) {
            LOG_WARN("[Sys_Animation] Entity " << id << " has invalid animHandle=0");
        }
    });
}

void Sys_Animation::OnFixedUpdate(Registry& registry, float fixedDt) {
    // 动画时间推进在 OnUpdate 中处理（需要可变 dt）
}

void Sys_Animation::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
    auto& am = AssetManager::Instance();

    registry.view<C_D_Animation, C_D_MeshRenderer>().each(
        [&](EntityID id, C_D_Animation& anim, C_D_MeshRenderer& mr) {
            if (!anim.playing || anim.animHandle == 0) return;

            MeshAnimation* clip = am.GetAnimation(anim.animHandle);
            if (!clip) return;

            Mesh* mesh = am.GetMesh(mr.meshHandle);
            if (!mesh) return;

            // 推进时间
            float duration = clip->GetFrameCount() / clip->GetFrameRate();
            anim.time += dt * anim.speed;
            if (anim.loop) {
                anim.time = fmod(anim.time, duration);
                if (anim.time < 0.f) anim.time += duration;
            } else {
                anim.time = std::min(anim.time, duration);
                if (anim.time >= duration) anim.playing = false;
            }

            SampleAnimation(anim, clip, mesh);
        }
    );
}

void Sys_Animation::SampleAnimation(C_D_Animation& anim,
                                     MeshAnimation* clip,
                                     Mesh* mesh) {
    const int   frameCount  = (int)clip->GetFrameCount();
    const float frameRate   = clip->GetFrameRate();
    const int   jointCount  = (int)clip->GetJointCount();

    // 计算当前帧索引（线性插值两帧之间）
    float exactFrame = anim.time * frameRate;
    int   frameA     = (int)exactFrame % frameCount;
    int   frameB     = (frameA + 1) % frameCount;
    float t          = exactFrame - (float)(int)exactFrame;

    const std::vector<Matrix4>& invBind = mesh->GetInverseBindPose();
    int boneCount = std::min(jointCount, C_D_Animation::MAX_BONES);

    for (int j = 0; j < boneCount; j++) {
        // 获取两帧的关节矩阵（GetJointData(frame) 返回指向 jointCount 个矩阵的指针）
        const Matrix4* frameDataA = clip->GetJointData(frameA);
        const Matrix4* frameDataB = clip->GetJointData(frameB);
        const Matrix4& matA = frameDataA[j];
        const Matrix4& matB = frameDataB[j];

        // 线性混合（简化版，精确版应对四元数做 slerp）
        Matrix4 blended;
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                blended.array[col][row] = matA.array[col][row] * (1.f - t)
                                        + matB.array[col][row] * t;

        // 最终骨骼矩阵 = inverseBindPose × blendedLocalToWorld
        if (j < (int)invBind.size()) {
            anim.boneMatrices[j] = invBind[j] * blended;
        } else {
            anim.boneMatrices[j] = blended;
        }
    }

    // 剩余骨骼槽填充单位矩阵
    for (int j = boneCount; j < C_D_Animation::MAX_BONES; j++) {
        anim.boneMatrices[j] = Matrix4();
    }
}

} // namespace ECS
