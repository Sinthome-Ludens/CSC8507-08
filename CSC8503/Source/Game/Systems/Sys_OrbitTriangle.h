/**
 * @file Sys_OrbitTriangle.h
 * @brief 环绕三角形系统声明（优先级 101）：管理三角形的环绕、发射和命中。
 *
 * 在 OnFixedUpdate 中运行（Physics(100) 之后），确保读到的玩家 Transform
 * 是当前帧物理同步后的最新值，消除一帧延迟导致的跟随偏移。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

class Sys_OrbitTriangle : public ISystem {
public:
    /** @brief 初始化 Res_OrbitConfig 并加载三角形 mesh。 */
    void OnAwake(Registry& registry) override;
    /** @brief 弹簧阻尼环绕 + 弹射制导 + 命中击杀，Physics 同步后执行。 */
    void OnFixedUpdate(Registry& registry, float fixedDt) override;
    /** @brief 重置内部状态（延迟初始化标记与累计时间）。 */
    void OnDestroy(Registry& registry) override;

private:
    bool m_NeedInitialRebuild = true; ///< 首帧延迟初始化标记
    float m_GlobalTime = 0.0f;        ///< 累计时间（用于公转 + 悬浮动画）
};

} // namespace ECS
