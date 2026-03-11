/**
 * @file Sys_ImGuiEntityDebug.h
 * @brief 全量实体调试窗口系统声明。
 *
 * @details
 * 提供独立的 ImGui 调试窗口，列出 Registry 中所有存活实体，并展示选中实体的常见组件详情。
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 全量实体调试窗口系统。
 * @details 使用 Registry 的全量实体遍历接口收集当前场景所有活跃实体，并提供列表 + 详情双栏调试视图。
 */
class Sys_ImGuiEntityDebug : public ISystem {
public:
    /**
     * @brief 初始化全量实体调试窗口系统。
     * @param registry 当前场景注册表
     */
    void OnAwake(Registry& registry) override;

    /**
     * @brief 根据 UI 标志渲染全量实体调试窗口。
     * @param registry 当前场景注册表
     * @param dt 本帧时间步长
     */
    void OnUpdate(Registry& registry, float dt) override;

    /**
     * @brief 销毁全量实体调试窗口系统。
     * @param registry 当前场景注册表
     */
    void OnDestroy(Registry& registry) override;

private:
    /**
     * @brief 渲染实体调试窗口主体。
     * @details 左栏列出所有活跃实体，右栏展示当前选中实体的组件详情。
     * @param registry 当前场景注册表
     */
    void RenderEntityDebugWindow(Registry& registry);

    /**
     * @brief 渲染实体列表栏。
     * @details 从所有活跃实体中生成可选列表，并显示常见组件存在情况。
     * @param registry 当前场景注册表
     */
    void RenderEntityList(Registry& registry);

    /**
     * @brief 渲染选中实体的详情栏。
     * @details 展示实体有效性以及若干常见组件的字段快照；若实体无效则给出提示。
     * @param registry 当前场景注册表
     */
    void RenderEntityDetails(Registry& registry);

    EntityID m_SelectedEntity = Entity::NULL_ENTITY;
};

} // namespace ECS
#endif
