/**
 * @file Scene_HangerB.cpp
 * @brief HangerB level scene lifecycle.
 */
#include "Scene_HangerB.h"
#include "GameplayBootstrap.h"

#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/Res_Network.h"
#include "Game/Systems/SystemPriorities.h"
#include "Game/Systems/Sys_Navigation.h"
#include "Game/Utils/Log.h"

void Scene_HangerB::OnEnter(ECS::Registry&          registry,
                            ECS::SystemManager&     systems,
                            const Res_NCL_Pointers& /*nclPtrs*/)
{
    const bool isMultiplayer = registry.has_ctx<ECS::Res_Network>()
        && registry.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;

    ECS::AssetManager::Instance().Init();
    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    ECS::GameplaySceneConfig config{};
    config.sceneName     = "Scene_HangerB";
    config.mapConfigJson = "Prefab_Map_HangerB.json";
    config.isMultiplayer = isMultiplayer;

    ECS::BootstrapEmplaceCtx(registry, this, cubeMesh, config);
    ECS::BootstrapRegisterSystems(systems, config);

    auto mapResult = ECS::BootstrapLoadMap(registry, cubeMesh, config, m_Pathfinder);

    auto* navSys = systems.Register<ECS::Sys_Navigation>(ECS::Priority::Navigation);
    navSys->SetPathfinder(m_Pathfinder.get());

    systems.AwakeAll(registry);

    ECS::BootstrapPostAwake(registry, config);

    LOG_INFO("[Scene_HangerB] OnEnter complete. " << systems.Count() << " systems awake.");
}

void Scene_HangerB::OnExit(ECS::Registry&      registry,
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

    LOG_INFO("[Scene_HangerB] OnExit complete. All systems destroyed.");
}
