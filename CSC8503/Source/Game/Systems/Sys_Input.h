/**
 * @file Sys_Input.h
 * @brief 输入系统：每帧从 NCL 窗口同步输入状态到 ECS 全局资源 Res_Input（优先级 10）
 *
 * @details
 * `Sys_Input` 是 ECS 管线的入口系统之一，负责在所有逻辑系统运行前，
 * 调用 `InputAdapter` 将物理设备的输入快照同步到 `Res_Input`。
 * 保证所有后续系统读取到同一帧的输入快照。
 *
 * 写：Res_Input（Registry ctx）
 *
 * @see Res_Input
 * @see InputAdapter
 */

#pragma once

#include "Core/ECS/SystemManager.h" 

namespace ECS {

class Sys_Input : public ISystem {
public:
    /**
     * @brief 初始化输入资源
     * @param registry ECS 注册表
     */
    void OnAwake(Registry& registry) override;

    /**
     * @brief 每帧同步输入状态
     * @param registry ECS 注册表
     * @param dt 帧耗时
     */
    void OnUpdate(Registry& registry, float dt) override;

    void OnDestroy(Registry& registry) override {}
};

} // namespace ECS