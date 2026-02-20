#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_MeshRenderer.h"
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
 * ## 工作原理（桥接模式）
 *
 * GameTechRenderer 只渲染 GameWorld::gameObjects 列表中的对象。
 * Sys_Render 通过维护一张 EntityID → NCL::GameObject* 代理表，
 * 将 ECS 实体的渲染状态同步到 NCL 渲染管线：
 *
 * ```
 * ECS 实体 (C_D_Transform + C_D_MeshRenderer)
 *     │
 *     └─ Sys_Render::OnUpdate()
 *           ├─ 新实体 → CreateProxy() → GameWorld::AddGameObject()
 *           ├─ 已有  → SyncProxy()   → NCL::Transform 同步
 *           └─ 已销毁 → CleanupOrphans() → GameWorld::RemoveGameObject()
 *
 * GameTechRenderer::RenderFrame() 自动渲染 GameWorld 中的所有对象
 * ```
 *
 * ## 默认构造
 *
 * 满足 SystemManager::Register<T>() 的默认构造要求。
 * GameWorld* 通过 Registry Context 中的 Res_NCL_Pointers 获取。
 *
 * @see C_D_Transform
 * @see C_D_MeshRenderer
 * @see AssetManager
 */
class Sys_Render : public ISystem {
public:
    Sys_Render() = default;

    void OnAwake  (Registry& registry) override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override;

private:
    NCL::CSC8503::GameWorld* m_GameWorld = nullptr;

    /// EntityID → NCL 代理 GameObject（所有权不在此，由 GameWorld 管理）
    std::unordered_map<EntityID, NCL::CSC8503::GameObject*> m_ProxyObjects;

    /// 为新 ECS 实体创建代理 GameObject 并注册到 GameWorld
    void CreateProxy(Registry& reg, EntityID id,
                     const C_D_Transform& tf, const C_D_MeshRenderer& mr);

    /// 同步 C_D_Transform 数据到代理 GameObject 的 NCL Transform
    void SyncProxy(NCL::CSC8503::GameObject* proxy, const C_D_Transform& tf);

    /// 移除所有对应实体已无效的代理对象
    void CleanupOrphans(Registry& reg);
};

} // namespace ECS
