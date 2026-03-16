/**
 * @file C_D_Animation.h
 * @brief 骨骼动画组件：存储当前播放状态和每帧骨骼矩阵。
 *
 * @details
 * - `animHandle`：AssetManager 中的动画剪辑句柄
 * - `boneMatrices[MAX_BONES]`：由 Sys_Animation 每帧写入，Sys_Render 读取并上传 GPU
 * - 组件体积较大（~6KB），但这是计划约定的设计；Sys_Animation 集中写入以最大化缓存利用率
 */
#pragma once

#include <cstdint>
#include "Matrix.h"
#include "Game/Components/C_D_MeshRenderer.h" // AnimHandle 定义

namespace ECS {

/// @brief 骨骼动画数据组件（Sys_Animation 写，Sys_Render 读）
struct C_D_Animation {
    AnimHandle animHandle = 0;   ///< AssetManager 动画剪辑句柄（0=无动画）
    float      time       = 0.f; ///< 当前播放时间（秒）
    float      speed      = 1.f; ///< 播放速度倍数
    bool       loop       = true;  ///< 是否循环
    bool       playing    = true;  ///< 是否正在播放

    static constexpr int MAX_BONES = 96; ///< 最大骨骼数（匹配 GPU uniform 数组）

    /// @brief 当前帧骨骼矩阵（由 Sys_Animation 写入，Sys_Render 上传 boneMatrices uniform）
    NCL::Maths::Matrix4 boneMatrices[MAX_BONES];
};

} // namespace ECS
