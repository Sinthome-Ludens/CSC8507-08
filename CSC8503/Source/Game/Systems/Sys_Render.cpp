#include "Sys_Render.h"
#include "Game/Utils/Log.h"
#include "Matrix.h"
#include "OGLMesh.h"   // 需要完整定义才能做 OGLMesh* → Mesh* 的隐式转换

using namespace NCL;
using namespace NCL::Maths;
using namespace NCL::CSC8503;
using namespace NCL::Rendering;

namespace ECS {

// ============================================================
// OnAwake
// ============================================================
void Sys_Render::OnAwake(Registry& registry) {
    GAME_ASSERT(registry.has_ctx<Res_NCL_Pointers>(),
                "[Sys_Render] Res_NCL_Pointers not in context. Register it before AwakeAll.");

    m_GameWorld = registry.ctx<Res_NCL_Pointers>().world;

    GAME_ASSERT(m_GameWorld != nullptr,
                "[Sys_Render] GameWorld pointer is null in Res_NCL_Pointers.");

    LOG_INFO("[Sys_Render] OnAwake - bridge to GameWorld ready");
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_Render::OnUpdate(Registry& registry, float dt) {
    if (!m_GameWorld) return;

    // --- 1. 遍历所有可渲染实体：创建代理或同步 Transform ---
    registry.view<C_D_Transform, C_D_MeshRenderer>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_MeshRenderer& mr) {
            auto it = m_ProxyObjects.find(id);
            if (it == m_ProxyObjects.end()) {
                CreateProxy(registry, id, tf, mr);
            } else {
                SyncProxy(it->second, tf);
            }
        }
    );

    // --- 2. 清理已销毁实体的代理 ---
    CleanupOrphans(registry);
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_Render::OnDestroy(Registry& registry) {
    if (!m_GameWorld) return;

    for (auto& [id, proxy] : m_ProxyObjects) {
        m_GameWorld->RemoveGameObject(proxy, /*andDelete=*/true);
    }
    m_ProxyObjects.clear();

    LOG_INFO("[Sys_Render] OnDestroy - all proxies removed");
}

// ============================================================
// CreateProxy
// ============================================================
void Sys_Render::CreateProxy(Registry& reg, EntityID id,
                              const C_D_Transform& tf, const C_D_MeshRenderer& mr)
{
    // 解析 Mesh Handle → OGLMesh*（is-a Mesh*，可直接传给 RenderObject）
    auto* mesh = AssetManager::Instance().GetMesh(mr.meshHandle);
    if (!mesh) {
        LOG_ERROR("[Sys_Render] Cannot create proxy for entity " << id << ": mesh is null");
        return;
    }

    // 创建代理 GameObject（不含 PhysicsObject，只用于渲染）
    auto* proxy = new NCL::CSC8503::GameObject("ECS_" + std::to_string(id));

    proxy->GetTransform()
        .SetPosition(tf.position)
        .SetScale(tf.scale)
        .SetOrientation(tf.rotation);

    // 默认不透明材质（无贴图；GameTechRenderer 使用 defaultShader）
    GameTechMaterial mat{};
    mat.type       = MaterialType::Opaque;
    mat.diffuseTex = nullptr;
    mat.bumpTex    = nullptr;

    proxy->SetRenderObject(
        new NCL::CSC8503::RenderObject(proxy->GetTransform(), mesh, mat));

    m_GameWorld->AddGameObject(proxy);
    m_ProxyObjects[id] = proxy;

    // 发布代理创建事件（上下文存在且指针有效时才发布，避免空指针访问）
    if (reg.has_ctx<ECS::EventBus*>()) {
        ECS::EventBus* bus = reg.ctx<ECS::EventBus*>();
        if (bus) {
            bus->publish(Evt_Render_ProxyCreated{id});
        }
    }

    LOG_INFO("[Sys_Render] Proxy created for entity " << id);
}

// ============================================================
// SyncProxy
// ============================================================
void Sys_Render::SyncProxy(NCL::CSC8503::GameObject* proxy, const C_D_Transform& tf) {
    proxy->GetTransform()
        .SetPosition(tf.position)
        .SetScale(tf.scale)
        .SetOrientation(tf.rotation);
}

// ============================================================
// CleanupOrphans
// ============================================================
void Sys_Render::CleanupOrphans(Registry& reg) {
    std::vector<EntityID> toRemove;

    for (auto& [id, proxy] : m_ProxyObjects) {
        if (!reg.Valid(id)) {
            toRemove.push_back(id);
        }
    }

    if (toRemove.empty()) return;

    // 读取 EventBus 指针时统一做空值保护，兼容场景销毁后的断链状态
    ECS::EventBus* bus = reg.has_ctx<ECS::EventBus*>()
                       ? reg.ctx<ECS::EventBus*>()
                       : nullptr;

    for (EntityID id : toRemove) {
        m_GameWorld->RemoveGameObject(m_ProxyObjects[id], /*andDelete=*/true);

        if (bus) {
            bus->publish(Evt_Render_ProxyDestroyed{id});
        }

        m_ProxyObjects.erase(id);
        LOG_INFO("[Sys_Render] Proxy destroyed for entity " << id);
    }
}

} // namespace ECS
