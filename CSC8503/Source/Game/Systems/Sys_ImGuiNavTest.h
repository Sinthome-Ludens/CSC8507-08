#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief NavTest 场景调试控制 ImGui 面板系统
 *
 * 职责：
 *   - 渲染导航敌人生成/删除控制面板（依赖 Res_NavTestState context）
 *   - 渲染导航目标生成/删除控制面板
 *   - 提供批量设置 detectionValue 的调试工具（使敌人进入 Hunt 状态追踪目标）
 *
 * 场景专属：依赖 Res_NavTestState，仅用于 NavTest 场景。
 * 系统无状态，所有数据存储于 Res_NavTestState context。
 *
 * 执行优先级：310（ImGui 300 之后）
 */
class Sys_ImGuiNavTest : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    void RenderNavTestWindow (Registry& registry);
    void SpawnEnemy_Nav      (Registry& registry);
    void DeleteLastEnemy_Nav (Registry& registry);
    void SpawnTarget         (Registry& registry);
    void DeleteLastTarget    (Registry& registry);

    bool m_ShowWindow = true;
};

} // namespace ECS
#endif
