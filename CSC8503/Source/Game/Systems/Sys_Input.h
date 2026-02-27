#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 输入同步系统（优先级 10）
 *
 * 职责：每帧调用 InputAdapter::Update() 将 NCL 输入同步到 Res_Input。
 * 保证所有后续系统读取到同一帧的输入快照。
 *
 * 写：Res_Input（Registry ctx）
 */
class Sys_Input : public ISystem {
public:
    void OnAwake (Registry& registry) override;
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
