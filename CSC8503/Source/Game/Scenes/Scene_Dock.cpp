/**
 * @file Scene_Dock.cpp
 * @brief Dock level scene lifecycle (resource loading, entity creation, system registration).
 */
#include "Scene_Dock.h"
#include "GameplayBootstrap.h"

#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/Res_Network.h"
#include "Game/Systems/SystemPriorities.h"
#include "Game/Systems/Sys_Navigation.h"
#include "Game/Utils/Log.h"

void Scene_Dock::OnEnter(ECS::Registry&          registry,
                         ECS::SystemManager&     systems,
                         const Res_NCL_Pointers& /*nclPtrs*/)
{
    const bool isMultiplayer = registry.has_ctx<ECS::Res_Network>()
        && registry.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;

    ECS::AssetManager::Instance().Init();
    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    ECS::GameplaySceneConfig config{};
    config.sceneName      = "Scene_Dock";
    config.mapConfigJson  = "Prefab_Map_Dock.json";
    config.isMultiplayer  = isMultiplayer;

    ECS::BootstrapEmplaceCtx(registry, this, cubeMesh, config);
    ECS::BootstrapRegisterSystems(systems, config);

    auto mapResult = ECS::BootstrapLoadMap(registry, cubeMesh, config, m_Pathfinder);

    auto* navSys = systems.Register<ECS::Sys_Navigation>(ECS::Priority::Navigation);
    navSys->SetPathfinder(m_Pathfinder.get());

    systems.AwakeAll(registry);

    ECS::BootstrapPostAwake(registry, config);

    LOG_INFO("[Scene_Dock] OnEnter complete. " << systems.Count() << " systems awake.");
}

void Scene_Dock::OnExit(ECS::Registry&      registry,
                        ECS::SystemManager& systems)
{
    const bool isMultiplayer = registry.has_ctx<ECS::Res_Network>()
        && registry.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;

    systems.DestroyAll(registry);

    ECS::GameplaySceneConfig config{};
    config.isMultiplayer = isMultiplayer;
    ECS::BootstrapEraseCtx(registry, config);

    m_Pathfinder.reset();
    registry.Clear();

    LOG_INFO("[Scene_Dock] OnExit complete. All systems destroyed.");
}
