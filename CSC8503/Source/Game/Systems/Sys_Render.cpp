/**
 * @file Sys_Render.cpp
 * @brief ECS 渲染桥接系统实现：将 ECS 实体同步为 NCL GameTechRenderer 代理对象。
 *
 * @details
 * - OnAwake：从 Res_NCL_Pointers 获取 GameWorld 指针
 * - OnUpdate：遍历 C_D_Transform + C_D_MeshRenderer 实体，CreateProxy/SyncProxy；
 *             调用 CleanupOrphans 删除已销毁实体的代理对象
 * - OnDestroy：移除并释放所有代理对象
 * - CreateProxy：为 ECS 实体在 GameWorld 中创建 NCL GameObject 代理
 * - SyncProxy：将 ECS Transform/Material 同步到已有代理的 NCL GameObject
 * - CleanupOrphans：检测并移除 ECS 中已销毁但代理仍存在的 GameObject
 */
#include "Sys_Render.h"
#include "Game/Components/C_D_DeathVisual.h"
#include "Game/Utils/Log.h"
#include "Matrix.h"
#include "OGLMesh.h"

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

    registry.view<C_D_Transform, C_D_MeshRenderer>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_MeshRenderer& mr) {
            auto it = m_ProxyObjects.find(id);
            if (it == m_ProxyObjects.end()) {
                CreateProxy(registry, id, tf, mr);
            } else {
                SyncProxy(registry, id, it->second, tf);
            }
        }
    );

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
// 辅助：将 ECS C_D_Material 同步到 NCL GameTechMaterial
// ============================================================
static void SyncMaterial(GameTechMaterial& nclMat, const C_D_Material& ecsMat) {
    nclMat.shadingModel     = static_cast<NCL::CSC8503::ShadingModel>((int)ecsMat.shadingModel);
    nclMat.metallic         = ecsMat.metallic;
    nclMat.roughness        = ecsMat.roughness;
    nclMat.ao               = ecsMat.ao;
    nclMat.emissiveColor    = ecsMat.emissiveColor;
    nclMat.emissiveStrength = ecsMat.emissiveStrength;
    nclMat.rimPower         = ecsMat.rimPower;
    nclMat.rimStrength      = ecsMat.rimStrength;
    nclMat.flatShading      = ecsMat.flatShading;
}

// ============================================================
// CreateProxy
// ============================================================
void Sys_Render::CreateProxy(Registry& reg, EntityID id,
                              const C_D_Transform& tf, const C_D_MeshRenderer& mr)
{
    auto* mesh = AssetManager::Instance().GetMesh(mr.meshHandle);
    if (!mesh) {
        LOG_ERROR("[Sys_Render] Cannot create proxy for entity " << id << ": mesh is null");
        return;
    }

    auto* proxy = new NCL::CSC8503::GameObject("ECS_" + std::to_string(id));

    proxy->GetTransform()
        .SetPosition(tf.position)
        .SetScale(tf.scale)
        .SetOrientation(tf.rotation);

    GameTechMaterial mat{};
    mat.type       = MaterialType::Opaque;
    mat.diffuseTex = nullptr;
    mat.bumpTex    = nullptr;

    // 同步 ECS 材质参数（如果有 C_D_Material 组件）
    if (reg.Has<C_D_Material>(id)) {
        const auto& ecsMat = reg.Get<C_D_Material>(id);
        SyncMaterial(mat, ecsMat);
    }

    auto* ro = new NCL::CSC8503::RenderObject(proxy->GetTransform(), mesh, mat);

    // 同步 baseColour 到 RenderObject（objectColour uniform）
    if (reg.Has<C_D_Material>(id)) {
        ro->SetColour(reg.Get<C_D_Material>(id).baseColour);
    }

    proxy->SetRenderObject(ro);

    m_GameWorld->AddGameObject(proxy);
    m_ProxyObjects[id] = proxy;

    if (reg.has_ctx<ECS::EventBus*>()) {
        reg.ctx<ECS::EventBus*>()->publish(Evt_Render_ProxyCreated{id});
    }

    LOG_INFO("[Sys_Render] Proxy created for entity " << id);
}

// ============================================================
// SyncProxy
// ============================================================
void Sys_Render::SyncProxy(Registry& reg, EntityID id,
                            NCL::CSC8503::GameObject* proxy, const C_D_Transform& tf)
{
    proxy->GetTransform()
        .SetPosition(tf.position)
        .SetScale(tf.scale)
        .SetOrientation(tf.rotation);

    // 每帧同步材质参数（支持 ImGui 实时调参）
    if (reg.Has<C_D_Material>(id)) {
        auto* ro = proxy->GetRenderObject();
        if (ro) {
            const auto& ecsMat = reg.Get<C_D_Material>(id);
            SyncMaterial(ro->GetMaterial(), ecsMat);
            ro->SetColour(ecsMat.baseColour);
        }
    }

    // 死亡视觉覆盖：colour + 透明材质
    if (reg.Has<C_D_DeathVisual>(id)) {
        const auto& dv = reg.Get<C_D_DeathVisual>(id);
        auto* ro = proxy->GetRenderObject();
        if (ro) {
            ro->SetColour(dv.colourOverride);
            if (dv.useTransparent) {
                ro->GetMaterial().type = MaterialType::Transparent;
            }
        }
    }
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
