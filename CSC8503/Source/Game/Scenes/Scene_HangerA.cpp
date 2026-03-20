/**
 * @file Scene_HangerA.cpp
 * @brief HangerA level scene lifecycle (resource loading, entity creation, system registration).
 */
#include "Scene_HangerA.h"

#include <cstring>
#include <algorithm>
#include <vector>
#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/MapLoadConfig.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_Network.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Components/Res_AIConfig.h"
#include "Game/Components/Res_MinimapState.h"
#include "Game/Components/C_D_Transform.h"
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
#include "Game/Systems/Sys_Network.h"
#include "Game/Systems/Sys_Interpolation.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Systems/Sys_Item.h"
#include "Game/Systems/Sys_ItemEffects.h"
#include "Game/Systems/Sys_Door.h"
#include "Game/Systems/Sys_LevelGoal.h"
#include "Game/Systems/Sys_Audio.h"
#include "Game/Components/Res_AudioConfig.h"
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

// ============================================================
// OnEnter
// ============================================================
/** @brief Load map resources, register systems, initialize NavMesh floor and boundary colliders. */
void Scene_HangerA::OnEnter(ECS::Registry&          registry,
                            ECS::SystemManager&     systems,
                            const Res_NCL_Pointers& /*nclPtrs*/)
{
    const bool isMultiplayer = registry.has_ctx<ECS::Res_Network>()
        && registry.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;
    const bool hasActiveNetworkSession = isMultiplayer
        && registry.ctx<ECS::Res_Network>().host != nullptr
        && registry.ctx<ECS::Res_Network>().remotePeerConnected;

    // ── 1. Asset init ───────────────────────────────────────────────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    // ── 2. Context resources ────────────────────────────────────────────
    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
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

    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    registry.ctx_emplace<ECS::Res_DataOcean>();

    // ── 3. Map loading (MapLoadConfig driven) ───────────────────────────
    MapLoadConfig mapConfig{};
    if (!ECS::PrefabLoader::LoadMapConfig("Prefab_Map_HangerA.json", mapConfig)) {
        LOG_ERROR("[Scene_HangerA] Failed to load map config from Prefab_Map_HangerA.json");
        return;
    }

    auto mapResult = ECS::LoadMap(registry, mapConfig, cubeMesh);
    if (isMultiplayer && registry.has_ctx<ECS::Res_Network>()) {
        auto& resNet = registry.ctx<ECS::Res_Network>();
        if (resNet.multiplayerMode == ECS::MultiplayerMode::SameMapGhostRace) {
            resNet.localPlayerEntity = mapResult.playerEntity;
            resNet.remoteGhostEntity = ECS::Entity::NULL_ENTITY;
        }
    }

    // ── 4. System registration (priority ascending) ─────────────────────
    systems.Register<ECS::Sys_Input>           ( 10);
    systems.Register<ECS::Sys_Animation>       ( 50);
    if (isMultiplayer) {
        systems.Register<ECS::Sys_Network>     ( 54);
        systems.Register<ECS::Sys_Interpolation>( 56);
    }
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

    // ── Snap item pickups to NavMesh walkable surface ──
    for (ECS::EntityID pickupId : mapResult.itemPickups) {
        if (!registry.Valid(pickupId)) continue;
        auto& tf = registry.Get<ECS::C_D_Transform>(pickupId);
        NCL::Maths::Vector3 snapped;
        if (m_Pathfinder->SnapToNavMesh(tf.position, snapped)) {
            tf.position.x = snapped.x;
            tf.position.z = snapped.z;
            tf.position.y = snapped.y + 0.8f;
        }
    }

    // ── Cache NavMesh boundary edges for minimap ──
    {
        auto& minimap = registry.ctx_emplace<ECS::Res_MinimapState>();
        auto boundaryEdges = m_Pathfinder->GetBoundaryEdges();
        minimap.edgeCount = std::min(static_cast<int>(boundaryEdges.size()),
                                     ECS::Res_MinimapState::kMaxEdges);
        for (int i = 0; i < minimap.edgeCount; ++i) {
            minimap.edges[i] = {
                boundaryEdges[i].v0.x, boundaryEdges[i].v0.z,
                boundaryEdges[i].v1.x, boundaryEdges[i].v1.z
            };
            minimap.worldMinX = std::min({minimap.worldMinX, boundaryEdges[i].v0.x, boundaryEdges[i].v1.x});
            minimap.worldMaxX = std::max({minimap.worldMaxX, boundaryEdges[i].v0.x, boundaryEdges[i].v1.x});
            minimap.worldMinZ = std::min({minimap.worldMinZ, boundaryEdges[i].v0.z, boundaryEdges[i].v1.z});
            minimap.worldMaxZ = std::max({minimap.worldMaxZ, boundaryEdges[i].v0.z, boundaryEdges[i].v1.z});
        }

        // Cache walkable triangles for minimap fill
        std::vector<NCL::Maths::Vector3> verts;
        std::vector<int> indices;
        m_Pathfinder->GetWalkableGeometry(verts, indices);
        int triCount = static_cast<int>(indices.size()) / 3;
        minimap.triangleCount = std::min(triCount, ECS::Res_MinimapState::kMaxTriangles);
        for (int t = 0; t < minimap.triangleCount; ++t) {
            const auto& v0 = verts[indices[t * 3]];
            const auto& v1 = verts[indices[t * 3 + 1]];
            const auto& v2 = verts[indices[t * 3 + 2]];
            minimap.triangles[t] = { v0.x, v0.z, v1.x, v1.z, v2.x, v2.z };
        }
    }

    systems.Register<ECS::Sys_PlayerCamera>    (150);
    systems.Register<ECS::Sys_Camera>          (155);
    systems.Register<ECS::Sys_DataOcean>       (195);
    systems.Register<ECS::Sys_Render>          (200);
    systems.Register<ECS::Sys_Item>            (250);
    systems.Register<ECS::Sys_ItemEffects>     (260);
    systems.Register<ECS::Sys_Door>            (270);
    systems.Register<ECS::Sys_Audio>           (275);

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>             (300);
    systems.Register<ECS::Sys_ImGuiEntityDebug>  (305);
    systems.Register<ECS::Sys_ImGuiEnemyAI>      (310);
    systems.Register<ECS::Sys_ImGuiNavTest>      (315);
    systems.Register<ECS::Sys_ImGuiRenderDebug>  (320);
    systems.Register<ECS::Sys_Chat>              (450);
    systems.Register<ECS::Sys_UI>                (500);
#endif
    systems.Register<ECS::Sys_Countdown>          (350);

    // ── 5. Game state ───────────────────────────────────────────────────
    if (!registry.has_ctx<ECS::Res_GameState>()) {
        registry.ctx_emplace<ECS::Res_GameState>();
    }
    auto& gs = registry.ctx<ECS::Res_GameState>();
    const bool preserveMultiplayerState = isMultiplayer && gs.isMultiplayer;
    const auto preservedPhase = gs.matchPhase;
    const auto preservedResult = gs.matchResult;
    const uint8_t preservedRoundIndex = gs.currentRoundIndex;
    const uint8_t preservedLocalStage = gs.localStageProgress;
    const uint8_t preservedOpponentStage = gs.opponentStageProgress;
    gs = ECS::Res_GameState{};
    gs.isMultiplayer = isMultiplayer;
    if (isMultiplayer) {
        gs.matchPhase = preserveMultiplayerState
            ? preservedPhase
            : (hasActiveNetworkSession ? ECS::MatchPhase::Running : ECS::MatchPhase::WaitingForPeer);
        gs.matchResult = preserveMultiplayerState ? preservedResult : ECS::MatchResult::None;
        gs.currentRoundIndex = preserveMultiplayerState ? preservedRoundIndex : 0;
        gs.localStageProgress = preserveMultiplayerState ? preservedLocalStage : 0;
        gs.opponentStageProgress = preserveMultiplayerState ? preservedOpponentStage : 0;
        gs.localProgress = gs.localStageProgress;
        gs.opponentProgress = gs.opponentStageProgress;
        gs.matchJustStarted = !preserveMultiplayerState && hasActiveNetworkSession;
    } else {
        gs.matchPhase = ECS::MatchPhase::Finished;
    }
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>() && registry.ctx<ECS::Res_UIState>().campaignContinue) {
        gs.alertLevel = registry.ctx<ECS::Res_UIState>().campaignAlertLevel;
    }
#endif

    // ── 6. Awake all systems ────────────────────────────────────────────
    systems.AwakeAll(registry);

    // ── Audio state (must be AFTER AwakeAll — Res_AudioState created in Sys_Audio::OnAwake) ──
    if (registry.has_ctx<ECS::Res_AudioState>()) {
        auto& audio = registry.ctx<ECS::Res_AudioState>();
        audio.isGameplay   = true;
        audio.requestedBgm = ECS::BgmId::GameplayNormal;
        audio.bgmOverride  = false;
    }

    // ── 7. UI HUD + FadeIn ──────────────────────────────────────────────
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>()) {
        auto& ui = registry.ctx<ECS::Res_UIState>();
        ui.previousScreen       = ui.activeScreen;
        ui.activeScreen         = ECS::UIScreen::HUD;
        ui.pendingSceneRequest  = ECS::SceneRequest::None;
        ui.sceneRequestDispatched = false;
        ui.loadingWaitForSpawn    = false;
        ui.transitionSceneRequest = ECS::SceneRequest::None;
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

    // ── 8. Save/load + inventory + equipment sync ───────────────────────
    if (!isMultiplayer && ECS::HasSaveFile()) {
        ECS::LoadGame(registry, false);
        if (registry.has_ctx<ECS::Res_ItemInventory2>()) {
            auto& inv = registry.ctx<ECS::Res_ItemInventory2>();
#ifdef USE_IMGUI
            if (registry.has_ctx<ECS::Res_UIState>() && registry.ctx<ECS::Res_UIState>().campaignContinue) {
                const auto& ui_ref = registry.ctx<ECS::Res_UIState>();
                const int limit = std::min(inv.kItemCount,
                                           static_cast<int>(std::size(ui_ref.campaignCarried)));
                for (int i = 0; i < limit; ++i)
                    inv.slots[i].carriedCount = ui_ref.campaignCarried[i];
            } else
#endif
            {
                inv.OnRoundStart();
            }
        }
    }
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>()) {
        registry.ctx<ECS::Res_UIState>().campaignContinue = false;
    }
#endif

    ECS::SyncEquipmentToGameState(registry);

    LOG_INFO("[Scene_HangerA] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit
// ============================================================
/** @brief Unregister all systems and release scene-specific resources. */
void Scene_HangerA::OnExit(ECS::Registry&      registry,
                           ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);
    const bool isMultiplayer = registry.has_ctx<ECS::Res_Network>()
        && registry.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;
    if (!isMultiplayer) {
        ECS::SaveGame(registry);
    }
    m_Pathfinder.reset();

    if (registry.has_ctx<IScene*>()) {
        registry.ctx_erase<IScene*>();
    }

    if (registry.has_ctx<Res_UIFlags>())              registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<Res_NavTestState>())          registry.ctx_erase<Res_NavTestState>();
    if (registry.has_ctx<ECS::Res_CQCConfig>())       registry.ctx_erase<ECS::Res_CQCConfig>();
    if (registry.has_ctx<ECS::Res_DeathConfig>())     registry.ctx_erase<ECS::Res_DeathConfig>();
    if (registry.has_ctx<ECS::Res_VisionConfig>())    registry.ctx_erase<ECS::Res_VisionConfig>();
    if (registry.has_ctx<ECS::Res_AIConfig>())        registry.ctx_erase<ECS::Res_AIConfig>();
    if (registry.has_ctx<ECS::Res_DataOcean>())       registry.ctx_erase<ECS::Res_DataOcean>();
    if (registry.has_ctx<ECS::Res_ItemInventory2>())  registry.ctx_erase<ECS::Res_ItemInventory2>();
    if (registry.has_ctx<ECS::Res_RadarState>())      registry.ctx_erase<ECS::Res_RadarState>();
    if (registry.has_ctx<ECS::Res_MinimapState>())   registry.ctx_erase<ECS::Res_MinimapState>();
    if (!isMultiplayer && registry.has_ctx<ECS::Res_GameState>()) registry.ctx_erase<ECS::Res_GameState>();
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_ToastState>())      registry.ctx_erase<ECS::Res_ToastState>();
    // Res_ChatState preserved across maps (only erased in MainMenu)
    if (registry.has_ctx<ECS::Res_InventoryState>())  registry.ctx_erase<ECS::Res_InventoryState>();
    if (registry.has_ctx<ECS::Res_LobbyState>())      registry.ctx_erase<ECS::Res_LobbyState>();
    if (registry.has_ctx<ECS::Res_DialogueData>())    registry.ctx_erase<ECS::Res_DialogueData>();
#endif

    registry.Clear();

    LOG_INFO("[Scene_HangerA] OnExit complete. All systems destroyed.");
}
