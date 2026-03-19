/**
 * @file Scene_TutorialLevel.cpp
 * @brief Tutorial Level scene lifecycle (resource loading, entity creation, system registration).
 */
#include "Scene_TutorialLevel.h"

#include <cstring>
#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/MapLoadConfig.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Components/Res_AIConfig.h"
#include "Game/Components/Res_DataOcean.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_Countdown.h"
#include "Game/Systems/Sys_DataOcean.h"
#include "Game/Systems/Sys_DeathJudgment.h"
#include "Game/Systems/Sys_DeathEffect.h"
#include "Game/Systems/Sys_Input.h"
#include "Game/Systems/Sys_InputDispatch.h"
#include "Game/Systems/Sys_Animation.h"
#include "Game/Systems/Sys_PlayerDisguise.h"
#include "Game/Systems/Sys_PlayerStance.h"
#include "Game/Systems/Sys_StealthMetrics.h"
#include "Game/Systems/Sys_Movement.h"
#include "Game/Systems/Sys_PlayerCQC.h"
#include "Game/Systems/Sys_PlayerCamera.h"
#include "Game/Systems/Sys_EnemyAI.h"
#include "Game/Systems/Sys_EnemyVision.h"
#include "Game/Systems/Sys_Navigation.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Systems/Sys_Item.h"
#include "Game/Systems/Sys_ItemEffects.h"
#include "Game/Systems/Sys_Door.h"
#include "Game/Systems/Sys_LevelGoal.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/MapLoader.h"
#include "Game/Utils/PrefabLoader.h"
#include "Game/Utils/SaveManager.h"
#include "Game/Utils/ItemEquipSync.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiEntityDebug.h"
#include "Game/Systems/Sys_ImGuiEnemyAI.h"
#include "Game/Systems/Sys_ImGuiNavTest.h"
#include "Game/Systems/Sys_ImGuiRenderDebug.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Systems/Sys_Chat.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Components/Res_RadarState.h"
#include "Game/UI/UI_Toast.h"
#endif

/** @brief Load map resources, register systems, initialize NavMesh floor and boundary colliders. */
void Scene_TutorialLevel::OnEnter(ECS::Registry&          registry,
                                  ECS::SystemManager&     systems,
                                  const Res_NCL_Pointers& /*nclPtrs*/)
{
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    // Force tutorial dialogue tree (treeId "0") for this scene
    if (!registry.has_ctx<ECS::Res_ChatState>()) {
        registry.ctx_emplace<ECS::Res_ChatState>();
    }
    {
        auto& cs = registry.ctx<ECS::Res_ChatState>();
        cs.forcedTreeId[0] = '0';
        cs.forcedTreeId[1] = '\0';
    }
    if (!registry.has_ctx<ECS::Res_CQCConfig>()) {
        registry.ctx_emplace<ECS::Res_CQCConfig>(ECS::Res_CQCConfig{});
    }
    if (!registry.has_ctx<ECS::Res_DeathConfig>()) {
        registry.ctx_emplace<ECS::Res_DeathConfig>(ECS::Res_DeathConfig{});
    }
    registry.ctx_emplace<IScene*>(static_cast<IScene*>(this));
    if (!registry.has_ctx<ECS::Res_VisionConfig>()) {
        registry.ctx_emplace<ECS::Res_VisionConfig>(ECS::Res_VisionConfig{});
    }

    if (!registry.has_ctx<ECS::Res_AIConfig>()) {
        registry.ctx_emplace<ECS::Res_AIConfig>(ECS::Res_AIConfig{});
    }

    // 无条件重置：场景重进时 DestroyAll 已销毁旧实体，ctx 中残留的实体 ID 列表
    // 若不清空会导致 "Delete Last" 操作访问已失效 ID
    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    registry.ctx_emplace<ECS::Res_DataOcean>();

    MapLoadConfig mapConfig{};
    if (!ECS::PrefabLoader::LoadMapConfig("Prefab_Map_TutorialLevel.json", mapConfig)) {
        LOG_ERROR("[Scene_TutorialLevel] Failed to load map config from Prefab_Map_TutorialLevel.json");
        return;
    }

    auto mapResult = ECS::LoadMap(registry, mapConfig, cubeMesh);

    systems.Register<ECS::Sys_Input>           ( 10);
    systems.Register<ECS::Sys_Animation>       ( 50);
    systems.Register<ECS::Sys_InputDispatch>   ( 55);
    systems.Register<ECS::Sys_PlayerDisguise>  ( 59);
    systems.Register<ECS::Sys_PlayerStance>    ( 60);
    systems.Register<ECS::Sys_StealthMetrics>  ( 62);
    systems.Register<ECS::Sys_PlayerCQC>       ( 63);
    systems.Register<ECS::Sys_Movement>        ( 65);
    systems.Register<ECS::Sys_Physics>         (100);
    systems.Register<ECS::Sys_EnemyVision>     (110);
    systems.Register<ECS::Sys_EnemyAI>         (120);
    systems.Register<ECS::Sys_DeathJudgment>   (125);
    systems.Register<ECS::Sys_DeathEffect>     (126);
    systems.Register<ECS::Sys_LevelGoal>       (127);

    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());
    m_Pathfinder->LoadNavMesh(mapResult.navmeshPath);
    m_Pathfinder->ScaleVertices(mapConfig.mapScale);
    m_Pathfinder->OffsetVertices(NCL::Maths::Vector3(0.0f, mapConfig.yOffset * mapConfig.mapScale, 0.0f));

    systems.Register<ECS::Sys_PlayerCamera>    (150);
    systems.Register<ECS::Sys_Camera>          (155);
    systems.Register<ECS::Sys_DataOcean>       (195);
    systems.Register<ECS::Sys_Render>          (200);
    systems.Register<ECS::Sys_Item>            (250);
    systems.Register<ECS::Sys_ItemEffects>     (260);
    systems.Register<ECS::Sys_Door>            (270);

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>             (300);
    systems.Register<ECS::Sys_ImGuiEntityDebug>  (305);
    systems.Register<ECS::Sys_ImGuiEnemyAI>      (310);
    systems.Register<ECS::Sys_ImGuiNavTest>      (315);
    systems.Register<ECS::Sys_ImGuiRenderDebug>  (420);
    systems.Register<ECS::Sys_Chat>              (450);
    systems.Register<ECS::Sys_UI>                (500);
#endif
    systems.Register<ECS::Sys_Countdown>          (350);

    if (!registry.has_ctx<ECS::Res_GameState>()) {
        registry.ctx_emplace<ECS::Res_GameState>();
    }

    systems.AwakeAll(registry);

#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>()) {
        auto& ui = registry.ctx<ECS::Res_UIState>();
        ui.previousScreen       = ui.activeScreen;
        ui.activeScreen         = ECS::UIScreen::HUD;
        ui.pendingSceneRequest  = ECS::SceneRequest::None;
        ui.sceneRequestDispatched = false;
        ui.transitionActive     = true;
        ui.transitionTimer      = 0.0f;
        ui.transitionDuration   = 0.5f;
        ui.transitionType       = 0;
        ui.gameCursorFree = false;
        ui.cursorVisible  = false;
        ui.cursorLocked   = true;
    }

    ECS::UI::PushToast(registry, "MISSION START", ECS::ToastType::Success, 2.5f);
#endif

    if (ECS::HasSaveFile()) {
        ECS::LoadGame(registry, false);
        if (registry.has_ctx<ECS::Res_ItemInventory2>()) {
            registry.ctx<ECS::Res_ItemInventory2>().OnRoundStart();
        }
    }

    ECS::SyncEquipmentToGameState(registry);

    LOG_INFO("[Scene_TutorialLevel] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

/** @brief Unregister all systems and release scene-specific resources. */
void Scene_TutorialLevel::OnExit(ECS::Registry&      registry,
                                 ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);
    ECS::SaveGame(registry);
    m_Pathfinder.reset();

    if (registry.has_ctx<IScene*>()) {
        registry.ctx_erase<IScene*>();
    }

    if (registry.has_ctx<Res_UIFlags>())              registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<Res_NavTestState>())          registry.ctx_erase<Res_NavTestState>();
    if (registry.has_ctx<ECS::Res_CQCConfig>())       registry.ctx_erase<ECS::Res_CQCConfig>();
    if (registry.has_ctx<ECS::Res_DeathConfig>())     registry.ctx_erase<ECS::Res_DeathConfig>();
    if (registry.has_ctx<ECS::Res_VisionConfig>())    registry.ctx_erase<ECS::Res_VisionConfig>();
    if (registry.has_ctx<ECS::Res_AIConfig>())       registry.ctx_erase<ECS::Res_AIConfig>();
    if (registry.has_ctx<ECS::Res_DataOcean>())      registry.ctx_erase<ECS::Res_DataOcean>();
    if (registry.has_ctx<ECS::Res_ItemInventory2>())  registry.ctx_erase<ECS::Res_ItemInventory2>();
    if (registry.has_ctx<ECS::Res_RadarState>())      registry.ctx_erase<ECS::Res_RadarState>();
    if (registry.has_ctx<ECS::Res_GameState>())       registry.ctx_erase<ECS::Res_GameState>();
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_ToastState>())      registry.ctx_erase<ECS::Res_ToastState>();
    // Res_ChatState preserved across maps (only erased in MainMenu)
    if (registry.has_ctx<ECS::Res_InventoryState>())  registry.ctx_erase<ECS::Res_InventoryState>();
    if (registry.has_ctx<ECS::Res_LobbyState>())      registry.ctx_erase<ECS::Res_LobbyState>();
    if (registry.has_ctx<ECS::Res_DialogueData>())    registry.ctx_erase<ECS::Res_DialogueData>();
#endif

    registry.Clear();

    LOG_INFO("[Scene_TutorialLevel] OnExit complete. All systems destroyed.");
}
