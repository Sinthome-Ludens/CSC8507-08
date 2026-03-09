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

#include <unordered_map>

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

private:
    NCL::CSC8503::GameWorld* m_GameWorld = nullptr;

    std::unordered_map<EntityID, NCL::CSC8503::GameObject*> m_ProxyObjects;

    void CreateProxy(Registry& reg, EntityID id,
                     const C_D_Transform& tf, const C_D_MeshRenderer& mr);

    void SyncProxy(Registry& reg, EntityID id,
                   NCL::CSC8503::GameObject* proxy, const C_D_Transform& tf);

    void CleanupOrphans(Registry& reg);
};

} // namespace ECS
