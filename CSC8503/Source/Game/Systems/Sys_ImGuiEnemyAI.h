#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief EnemyAI 状态监控 ImGui 面板系统（场景无关）
 *
 * 职责：
 *   - 以表格形式渲染所有 C_T_Enemy 实体的 AI 状态
 *   - 显示字段：Entity ID / 世界坐标 / AI State / Detection Value / Action（Kill 按钮）
 *
 * 依赖组件：C_T_Enemy, C_D_Transform, C_D_AIState, C_D_AIPerception
 * 场景无关：只要 Registry 中有敌人实体，本面板均可工作。
 *
 * 执行优先级：310（ImGui 通用层 300 之后）
 */
class Sys_ImGuiEnemyAI : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    /**
     * @brief 渲染 "Enemy Monitoring Station" 调试窗口（5 列表格：ID/坐标/状态/感知值/Kill按钮）。
     * @param registry ECS 注册表
     */
    void RenderEnemyMonitorWindow(Registry& registry);

    bool m_ShowWindow = true;
};

} // namespace ECS
#endif
