#pragma once

#include "Core/ECS/SystemManager.h"

namespace ECS {

/**
 * @brief 插值系统：负责对远程实体的 Transform 进行平滑插值。
 *
 * @details
 * 该系统会根据 `Res_Time` 的当前时间，在 `C_D_InterpBuffer` 中的快照之间进行线性插值（Lerp/Slerp），
 * 并将结果应用到 `C_D_Transform`。
 */
class Sys_Interpolation : public ISystem {
public:
    /**
     * @brief 执行插值逻辑
     * @param reg ECS 注册表
     * @param dt 帧耗时
     */
    void OnUpdate(Registry& reg, float dt) override;

private:
    float m_RenderDelay = 0.1f; // 默认 100ms 渲染延迟，用于处理网络抖动
};

} // namespace ECS
