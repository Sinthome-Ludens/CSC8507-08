#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_Material.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Events/Evt_Render_Proxy.h"

// NCL 类型（桥接渲染所需）
#include "GameObject.h"
#include "GameWorld.h"
#include "RenderObject.h"

#include <vector>

namespace ECS {

/**
 * @brief 渲染桥接系统：为每个 ECS 实体在 GameWorld 中维护代理 GameObject
 *
 * 同步 C_D_Transform + C_D_MeshRenderer + C_D_Material 到 NCL 代理。
 * 若实体无 C_D_Material，使用默认 BlinnPhong。
 */
class Sys_Render : public ISystem {
public:
    Sys_Render() = default;

    void OnAwake  (Registry& registry) override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override;

    /// @brief 获取独立管理的柱子代理列表（不在 GameWorld 中）
    const std::vector<NCL::CSC8503::GameObject*>& GetPillarProxies() const { return m_PillarProxies; }

private:
    NCL::CSC8503::GameWorld* m_GameWorld = nullptr;

    /// @brief EntityID.Index → proxy 指针（直接索引，O(1) 查找）
    std::vector<NCL::CSC8503::GameObject*> m_ProxyObjects;
    /// @brief EntityID.Index → 完整 EntityID（含 Version，用于 Valid 校验）
    std::vector<EntityID> m_ProxyEntityIDs;

    /// @brief 数据海洋柱子代理（不加入 GameWorld，避免 BuildObjectLists/Physics 遍历）
    std::vector<NCL::CSC8503::GameObject*> m_PillarProxies;

    /// @brief 渲染器接口指针（用于传递 ocean 数据）
    NCL::CSC8503::GameTechRendererInterface* m_Renderer = nullptr;

    /// @brief 上次传递给渲染器的柱子 proxy 数量（避免每帧重复 invalidate SSBO）
    size_t m_LastPillarProxyCount = 0;

    void CreateProxy(Registry& reg, EntityID id,
                     const C_D_Transform& tf, const C_D_MeshRenderer& mr);

    void SyncProxy(Registry& reg, EntityID id,
                   NCL::CSC8503::GameObject* proxy, const C_D_Transform& tf);

    void CleanupOrphans(Registry& reg);
};

} // namespace ECS
