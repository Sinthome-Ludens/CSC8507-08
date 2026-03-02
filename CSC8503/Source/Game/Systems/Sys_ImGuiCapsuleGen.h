#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 胶囊生成 ImGui 面板系统
 *
 * 职责：
 *   - 渲染胶囊生成/删除控制面板（依赖 Res_CapsuleState context）
 *   - 通过 PrefabFactory::CreatePhysicsCapsule() 生成胶囊实体
 *   - 删除最后生成的胶囊实体
 *
 * 系统无状态，所有数据（含面板可见性 show_window）存储于 Res_CapsuleState context。
 * 通过 registry.has_ctx<Res_CapsuleState>() 检测功能是否可用。
 *
 * 执行优先级：301（紧跟 Sys_ImGui 300 之后）
 */
class Sys_ImGuiCapsuleGen : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    void RenderCapsuleControlWindow(Registry& registry);
    void SpawnCapsule              (Registry& registry);
    void DeleteLastCapsule         (Registry& registry);
};

} // namespace ECS
#endif
