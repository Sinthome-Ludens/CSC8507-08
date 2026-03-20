/**
 * @file Scene_TutorialLevel.cpp
 * @brief Tutorial Level scene lifecycle.
 *
 * 与其他 gameplay scene 的差异：
 *   - forcedTreeId = "0"（强制教程对话树）
 *   - callAutoFillHUD = true
 *   - 无 multiplayer 逻辑
 */
#include "Scene_TutorialLevel.h"
#include "GameplayBootstrap.h"

#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Systems/SystemPriorities.h"
#include "Game/Systems/Sys_Navigation.h"
#include "Game/Utils/Log.h"

void Scene_TutorialLevel::OnEnter(ECS::Registry&          registry,
                                  ECS::SystemManager&     systems,
                                  const Res_NCL_Pointers& /*nclPtrs*/)
{
    ECS::AssetManager::Instance().Init();
    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    ECS::GameplaySceneConfig config{};
    config.sceneName       = "Scene_TutorialLevel";
    config.mapConfigJson   = "Prefab_Map_TutorialLevel.json";
    config.isMultiplayer   = false;
    config.forcedTreeId    = "0";
    config.callAutoFillHUD = true;

    ECS::BootstrapEmplaceCtx(registry, this, cubeMesh, config);
    ECS::BootstrapRegisterSystems(systems, config);

    auto mapResult = ECS::BootstrapLoadMap(registry, cubeMesh, config, m_Pathfinder);

    auto* navSys = systems.Register<ECS::Sys_Navigation>(ECS::Priority::Navigation);
    navSys->SetPathfinder(m_Pathfinder.get());

    systems.AwakeAll(registry);

    ECS::BootstrapPostAwake(registry, config);

    LOG_INFO("[Scene_TutorialLevel] OnEnter complete. " << systems.Count() << " systems awake.");
}

void Scene_TutorialLevel::OnExit(ECS::Registry&      registry,
                                 ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);

    ECS::GameplaySceneConfig config{};
    config.isMultiplayer = false;
    ECS::BootstrapEraseCtx(registry, config);

    m_Pathfinder.reset();
    registry.Clear();

    LOG_INFO("[Scene_TutorialLevel] OnExit complete. All systems destroyed.");
}
