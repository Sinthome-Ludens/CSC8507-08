/**
 * @file Sys_DeathEffect.h
 * @brief 死亡视觉特效系统声明（ECS 系统，优先级 126）。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 赛博朋克死亡视觉特效系统（优先级 126）。
 * @details
 * 每帧推进具有 C_D_Dying 的实体的四阶段死亡动画：
 *   1. 数字冲击  (0.00–0.15s) — 青色闪光 + rim 光
 *   2. 霓虹故障  (0.15–0.55s) — 三色高频闪烁 + 轴抖动
 *   3. 数据溶解  (0.55–1.00s) — 青色淡出 + Y 拉伸/XZ 收缩
 *   4. 最终崩塌  (1.00–1.20s) — 品红闪光 + 全轴收缩至 10%
 * 动画结束后销毁实体。在 Sys_DeathJudgment(125) 之后执行。
 */
class Sys_DeathEffect : public ISystem {
public:
    void OnAwake  (Registry& registry) override {}
    /**
     * @brief 每帧推进死亡动画四阶段（数字冲击→霓虹故障→数据溶解→最终崩塌），动画结束后销毁实体。
     * @param registry ECS 注册表
     * @param dt       帧时间（秒）
     */
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override {}
};

} // namespace ECS
