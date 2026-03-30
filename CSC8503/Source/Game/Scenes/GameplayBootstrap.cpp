/**
 * @file GameplayBootstrap.cpp
 * @brief Gameplay 场景统一引导实现——6 个 gameplay scene 的共同模板收敛于此。
 */
#include "GameplayBootstrap.h"
#include "IScene.h"

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
#include "Game/Systems/SystemPriorities.h"

// ── Systems ──
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
#include "Game/Systems/Sys_Spin.h"
#include "Game/Systems/Sys_PlayerCQC.h"
#include "Game/Systems/Sys_PlayerCamera.h"
#include "Game/Systems/Sys_EnemyAI.h"
#include "Game/Systems/Sys_EnemyVision.h"
#include "Game/Systems/Sys_Navigation.h"
#include "Game/Systems/Sys_Network.h"
#include "Game/Systems/Sys_Interpolation.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Systems/Sys_Item.h"
#include "Game/Systems/Sys_ItemEffects.h"
#include "Game/Systems/Sys_OrbitTriangle.h"
#include "Game/Systems/Sys_Door.h"
#include "Game/Systems/Sys_LevelGoal.h"
#include "Game/Systems/Sys_Audio.h"
#include "Game/Components/Res_AudioConfig.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/MapLoader.h"
#include "Game/Utils/PrefabLoader.h"
#include "Game/Utils/SaveManager.h"
#include "Game/Utils/ItemEquipSync.h"
#include "Game/Utils/NavMeshPathfinderUtil.h"

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

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// BootstrapEmplaceCtx
// ============================================================
void BootstrapEmplaceCtx(Registry& registry, ::IScene* scene, MeshHandle cubeMesh,
                          const GameplaySceneConfig& config) {
    if (!registry.has_ctx<Res_UIFlags>())
        registry.ctx_emplace<Res_UIFlags>();
    if (!registry.has_ctx<Res_CQCConfig>())
        registry.ctx_emplace<Res_CQCConfig>(Res_CQCConfig{});
    if (!registry.has_ctx<Res_DeathConfig>())
        registry.ctx_emplace<Res_DeathConfig>(Res_DeathConfig{});
    registry.ctx_emplace<::IScene*>(scene);
    if (!registry.has_ctx<Res_VisionConfig>())
        registry.ctx_emplace<Res_VisionConfig>(Res_VisionConfig{});
    if (!registry.has_ctx<Res_AIConfig>())
        registry.ctx_emplace<Res_AIConfig>(Res_AIConfig{});

    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    registry.ctx_emplace<Res_DataOcean>();

    // forcedTreeId（仅 Tutorial 使用）
#ifdef USE_IMGUI
    if (config.forcedTreeId) {
        if (!registry.has_ctx<Res_ChatState>())
            registry.ctx_emplace<Res_ChatState>();
        auto& cs = registry.ctx<Res_ChatState>();
        size_t len = std::strlen(config.forcedTreeId);
        if (len >= sizeof(cs.forcedTreeId)) len = sizeof(cs.forcedTreeId) - 1;
        std::memcpy(cs.forcedTreeId, config.forcedTreeId, len);
        cs.forcedTreeId[len] = '\0';
        cs.lockToForcedTree = true;
    }
#endif
}

// ============================================================
// BootstrapRegisterSystems
// ============================================================
void BootstrapRegisterSystems(SystemManager& systems, const GameplaySceneConfig& config) {
    namespace P = Priority;

    systems.Register<Sys_Input>           (P::Input);
    systems.Register<Sys_Animation>       (P::Animation);
    if (config.isMultiplayer) {
        systems.Register<Sys_Network>     (P::Network);
        systems.Register<Sys_Interpolation>(P::Interpolation);
    }
    systems.Register<Sys_InputDispatch>   (P::InputDispatch);
    systems.Register<Sys_PlayerDisguise>  (P::PlayerDisguise);
    systems.Register<Sys_PlayerStance>    (P::PlayerStance);
    systems.Register<Sys_StealthMetrics>  (P::StealthMetrics);
    systems.Register<Sys_PlayerCQC>       (P::PlayerCQC);
    systems.Register<Sys_Movement>        (P::Movement);
    systems.Register<Sys_OrbitTriangle>   (P::OrbitTriangle);
    systems.Register<Sys_Spin>            (P::Spin);
    systems.Register<Sys_Physics>         (P::Physics);
    systems.Register<Sys_EnemyVision>     (P::EnemyVision);
    systems.Register<Sys_EnemyAI>         (P::EnemyAI);
    systems.Register<Sys_DeathJudgment>   (P::DeathJudgment);
    systems.Register<Sys_DeathEffect>     (P::DeathEffect);
    systems.Register<Sys_LevelGoal>       (P::LevelGoal);

    // Navigation 需要返回指针给 scene 设置 pathfinder，在 BootstrapLoadMap 中注册
    // systems.Register<Sys_Navigation>(P::Navigation) — 在 BootstrapLoadMap 里

    systems.Register<Sys_PlayerCamera>    (P::PlayerCamera);
    systems.Register<Sys_Camera>          (P::Camera);
    systems.Register<Sys_DataOcean>       (P::DataOcean);
    systems.Register<Sys_Render>          (P::Render);
    systems.Register<Sys_Item>            (P::Item);
    systems.Register<Sys_ItemEffects>     (P::ItemEffects);
    systems.Register<Sys_Door>            (P::Door);
    systems.Register<Sys_Audio>           (P::Audio);

#ifdef USE_IMGUI
  #ifndef GAME_SHIPPING
    systems.Register<Sys_ImGui>             (P::ImGui);
    systems.Register<Sys_ImGuiEntityDebug>  (P::ImGuiEntityDebug);
    systems.Register<Sys_ImGuiEnemyAI>      (P::ImGuiEnemyAI);
    systems.Register<Sys_ImGuiNavTest>      (P::ImGuiNavTest);
    systems.Register<Sys_ImGuiRenderDebug>  (P::ImGuiRenderDebug);
  #endif
    systems.Register<Sys_Chat>              (P::Chat);
    systems.Register<Sys_UI>                (P::UI);
#endif
    systems.Register<Sys_Countdown>       (P::Countdown);
}

// ============================================================
// BootstrapLoadMap
// ============================================================
MapLoadResult BootstrapLoadMap(Registry& registry, MeshHandle cubeMesh,
                                const GameplaySceneConfig& config,
                                std::unique_ptr<NavMeshPathfinderUtil>& outPathfinder) {
    MapLoadConfig mapConfig{};
    if (!PrefabLoader::LoadMapConfig(config.mapConfigJson, mapConfig)) {
        LOG_ERROR("[" << config.sceneName << "] Failed to load map config from " << config.mapConfigJson);
        return {};
    }

    auto mapResult = LoadMap(registry, mapConfig, cubeMesh);

    // ── Orb 装饰球体 ──
    {
        auto& am = AssetManager::Instance();
        MeshHandle playerOrbInner = am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/playerIn.gltf");
        MeshHandle playerOrbOuter = am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/player.gltf");
        MeshHandle enemyOrbInner  = am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/enemyIn.gltf");
        MeshHandle enemyOrbOuter  = am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/enemy.gltf");

        PrefabFactory::CreatePlayerOrbs(registry, mapResult.playerEntity, playerOrbInner, playerOrbOuter);
        for (EntityID enemyId : mapResult.enemies) {
            PrefabFactory::CreateEnemyOrbs(registry, enemyId, enemyOrbInner, enemyOrbOuter);
        }
    }

    // ── Multiplayer 玩家实体注册 ──
    if (config.isMultiplayer && registry.has_ctx<Res_Network>()) {
        auto& resNet = registry.ctx<Res_Network>();
        if (resNet.multiplayerMode == MultiplayerMode::SameMapGhostRace) {
            resNet.localPlayerEntity = mapResult.playerEntity;
            resNet.remoteGhostEntity = Entity::NULL_ENTITY;
        }
    }

    // ── NavMesh + Navigation System ──
    SystemManager* sysMgr = nullptr;
    // Navigation 需要在这里注册以便拿到返回的指针
    // 由调用方在 BootstrapRegisterSystems 之后、这里之前不注册 Navigation
    // 我们在这里完成
    // 但 SystemManager 不在参数中——通过 registry ctx 获取不到
    // 改用 outPathfinder 让 scene 自行注册 Navigation
    outPathfinder = std::make_unique<NavMeshPathfinderUtil>();
    outPathfinder->LoadNavMesh(mapResult.navmeshPath);
    outPathfinder->ScaleVertices(mapConfig.mapScale);
    outPathfinder->OffsetVertices(Vector3(0.0f, mapConfig.yOffset * mapConfig.mapScale, 0.0f));

    // ── Snap item pickups to NavMesh ──
    for (EntityID pickupId : mapResult.itemPickups) {
        if (!registry.Valid(pickupId)) continue;
        auto& tf = registry.Get<C_D_Transform>(pickupId);
        Vector3 snapped;
        if (outPathfinder->SnapToNavMesh(tf.position, snapped)) {
            tf.position.x = snapped.x;
            tf.position.z = snapped.z;
            tf.position.y = snapped.y + mapConfig.pickupSnapYOffset;
        }
    }

    // ── Minimap boundary cache ──
    {
        auto& minimap = registry.ctx_emplace<Res_MinimapState>();
        auto boundaryEdges = outPathfinder->GetBoundaryEdges();
        minimap.edgeCount = std::min(static_cast<int>(boundaryEdges.size()),
                                     Res_MinimapState::kMaxEdges);
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

        std::vector<Vector3> verts;
        std::vector<int> indices;
        outPathfinder->GetWalkableGeometry(verts, indices);
        int triCount = static_cast<int>(indices.size()) / 3;
        minimap.triangleCount = std::min(triCount, Res_MinimapState::kMaxTriangles);
        for (int t = 0; t < minimap.triangleCount; ++t) {
            const auto& v0 = verts[indices[t * 3]];
            const auto& v1 = verts[indices[t * 3 + 1]];
            const auto& v2 = verts[indices[t * 3 + 2]];
            minimap.triangles[t] = { v0.x, v0.z, v1.x, v1.z, v2.x, v2.z };
        }
    }

    return mapResult;
}

// ============================================================
// BootstrapPostAwake
// ============================================================
void BootstrapPostAwake(Registry& registry, const GameplaySceneConfig& config) {
    // ── Res_GameState 初始化/保留 ──
#ifdef USE_IMGUI
    {
        const bool isMP = config.isMultiplayer;
        const bool hasActiveNet = isMP
            && registry.has_ctx<Res_Network>()
            && registry.ctx<Res_Network>().host != nullptr
            && registry.ctx<Res_Network>().remotePeerConnected;

        if (!registry.has_ctx<Res_GameState>())
            registry.ctx_emplace<Res_GameState>();

        auto& gs = registry.ctx<Res_GameState>();
        const bool preserveMP = isMP && gs.isMultiplayer;
        const auto savedPhase     = gs.matchPhase;
        const auto savedResult    = gs.matchResult;
        const uint8_t savedRound  = gs.currentRoundIndex;
        const uint8_t savedLocal  = gs.localStageProgress;
        const uint8_t savedOpp    = gs.opponentStageProgress;

        gs = Res_GameState{};
        gs.isMultiplayer = isMP;
        if (isMP) {
            gs.matchPhase = preserveMP ? savedPhase
                : (hasActiveNet ? MatchPhase::Running : MatchPhase::WaitingForPeer);
            gs.matchResult         = preserveMP ? savedResult : MatchResult::None;
            gs.currentRoundIndex   = preserveMP ? savedRound : 0;
            gs.localStageProgress  = preserveMP ? savedLocal : 0;
            gs.opponentStageProgress = preserveMP ? savedOpp : 0;
            gs.localProgress       = gs.localStageProgress;
            gs.opponentProgress    = gs.opponentStageProgress;
            gs.matchJustStarted    = !preserveMP && hasActiveNet;
        } else {
            gs.matchPhase = MatchPhase::Finished;
        }
        // Campaign persistence: 恢复跨地图警戒度
        if (registry.has_ctx<Res_UIState>() && registry.ctx<Res_UIState>().campaignContinue) {
            gs.alertLevel = registry.ctx<Res_UIState>().campaignAlertLevel;
        }
    }
#else
    if (!registry.has_ctx<Res_GameState>())
        registry.ctx_emplace<Res_GameState>();
#endif

    // ── Audio ──
    if (registry.has_ctx<Res_AudioState>()) {
        auto& audio = registry.ctx<Res_AudioState>();
        audio.isGameplay   = true;
        audio.requestedBgm = BgmId::GameplayNormal;
        audio.bgmOverride  = false;
    }

    // ── UI HUD reset ──
#ifdef USE_IMGUI
    if (registry.has_ctx<Res_UIState>()) {
        auto& ui = registry.ctx<Res_UIState>();
        ui.previousScreen         = ui.activeScreen;
        ui.activeScreen           = UIScreen::HUD;
        ui.pendingSceneRequest    = SceneRequest::None;
        ui.sceneRequestDispatched = false;
        ui.loadingWaitForSpawn    = false;
        ui.transitionSceneRequest = SceneRequest::None;
        ui.transitionActive       = true;
        ui.transitionTimer        = 0.0f;
        ui.transitionDuration     = 0.5f;
        ui.transitionType         = 0;
        ui.gameCursorFree = false;
        ui.cursorVisible  = false;
        ui.cursorLocked   = true;
    }

    UI::PushToast(registry, "MISSION START", ToastType::Success, 2.5f);
#endif

    // ── SaveManager + Campaign persistence ──
    if (!config.isMultiplayer && HasSaveFile()) {
        LoadGame(registry, false);
        if (registry.has_ctx<Res_ItemInventory2>()) {
            auto& inv = registry.ctx<Res_ItemInventory2>();
#ifdef USE_IMGUI
            // Campaign continuation: 恢复跨地图道具携带量
            if (registry.has_ctx<Res_UIState>() && registry.ctx<Res_UIState>().campaignContinue) {
                const auto& uiRef = registry.ctx<Res_UIState>();
                const int limit = std::min(inv.kItemCount,
                                           static_cast<int>(std::size(uiRef.campaignCarried)));
                for (int i = 0; i < limit; ++i)
                    inv.slots[i].carriedCount = uiRef.campaignCarried[i];
            } else
#endif
            {
                inv.OnRoundStart();
            }
        }
    }
#ifdef USE_IMGUI
    // 恢复完毕后清除 campaignContinue 标志
    if (registry.has_ctx<Res_UIState>()) {
        registry.ctx<Res_UIState>().campaignContinue = false;
    }
#endif

    // ── Equipment sync ──
    SyncEquipmentToGameState(registry);
    if (config.callAutoFillHUD) {
        AutoFillHUDSlots(registry);
    }
}

// ============================================================
// BootstrapEraseCtx
// ============================================================
void BootstrapEraseCtx(Registry& registry, const GameplaySceneConfig& config) {
    // ── SaveManager ──
    if (!config.isMultiplayer) {
        SaveGame(registry);
    }

    // ── 统一 ctx 清理列表 ──
    // 顺序不重要，但必须覆盖 BootstrapEmplaceCtx 注册的所有资源
    if (registry.has_ctx<::IScene*>())              registry.ctx_erase<::IScene*>();
    if (registry.has_ctx<Res_UIFlags>())            registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<Res_NavTestState>())       registry.ctx_erase<Res_NavTestState>();
    if (registry.has_ctx<Res_CQCConfig>())          registry.ctx_erase<Res_CQCConfig>();
    if (registry.has_ctx<Res_DeathConfig>())        registry.ctx_erase<Res_DeathConfig>();
    if (registry.has_ctx<Res_VisionConfig>())       registry.ctx_erase<Res_VisionConfig>();
    if (registry.has_ctx<Res_AIConfig>())           registry.ctx_erase<Res_AIConfig>();
    if (registry.has_ctx<Res_DataOcean>())          registry.ctx_erase<Res_DataOcean>();
    if (registry.has_ctx<Res_MinimapState>())       registry.ctx_erase<Res_MinimapState>();

    // 由 system OnAwake 创建的 ctx（需要在 DestroyAll 后清理）
    if (registry.has_ctx<Res_ItemInventory2>())     registry.ctx_erase<Res_ItemInventory2>();
    if (registry.has_ctx<Res_RadarState>())         registry.ctx_erase<Res_RadarState>();

    // Res_GameState：singleplayer 清理，multiplayer 保留
    if (!config.isMultiplayer && registry.has_ctx<Res_GameState>())
        registry.ctx_erase<Res_GameState>();

    // Res_ChatState：始终保留（跨 gameplay scene），只在 MainMenu 清理
    // 这是显式规则，不是遗漏

#ifdef USE_IMGUI
    if (registry.has_ctx<Res_ToastState>())         registry.ctx_erase<Res_ToastState>();
    if (registry.has_ctx<Res_InventoryState>())     registry.ctx_erase<Res_InventoryState>();
    if (registry.has_ctx<Res_LobbyState>())         registry.ctx_erase<Res_LobbyState>();
    if (registry.has_ctx<Res_DialogueData>())       registry.ctx_erase<Res_DialogueData>();
#endif
}

} // namespace ECS
