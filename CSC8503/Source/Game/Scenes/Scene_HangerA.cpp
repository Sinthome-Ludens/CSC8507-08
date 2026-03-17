/**
 * @file Scene_HangerA.cpp
 * @brief HangerA level scene lifecycle (resource loading, entity creation, system registration).
 */
#include "Scene_HangerA.h"

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
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_Countdown.h"
#include "Game/Systems/Sys_DeathJudgment.h"
#include "Game/Systems/Sys_DeathEffect.h"
#include "Game/Systems/Sys_Input.h"
#include "Game/Systems/Sys_InputDispatch.h"
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
#include "Game/Utils/SaveManager.h"

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

    // ── 3. Map loading (MapLoadConfig driven) ───────────────────────────
    MapLoadConfig mapConfig{};
    strncpy_s(mapConfig.renderMesh,    sizeof(mapConfig.renderMesh),    "HangerA.obj", _TRUNCATE);
    strncpy_s(mapConfig.collisionMesh, sizeof(mapConfig.collisionMesh), "HangerA_collision.obj", _TRUNCATE);
    strncpy_s(mapConfig.navmesh,       sizeof(mapConfig.navmesh),       "HangerA.navmesh", _TRUNCATE);
    strncpy_s(mapConfig.finishMesh,    sizeof(mapConfig.finishMesh),    "HangerA_finish.obj", _TRUNCATE);
    strncpy_s(mapConfig.startPoints,   sizeof(mapConfig.startPoints),   "HangerA.startpoints", _TRUNCATE);
    strncpy_s(mapConfig.enemySpawns,   sizeof(mapConfig.enemySpawns),   "HangerA.enemyspawns", _TRUNCATE);
    mapConfig.mapScale    = 1.0f;
    mapConfig.yOffset     = -6.0f;
    mapConfig.flipWinding = true;

    auto mapResult = ECS::LoadMap(registry, mapConfig, cubeMesh);

    // ── 4. System registration (priority ascending) ─────────────────────
    systems.Register<ECS::Sys_Input>           ( 10);
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
    systems.Register<ECS::Sys_Render>          (200);
    systems.Register<ECS::Sys_Item>            (250);
    systems.Register<ECS::Sys_ItemEffects>     (260);
    systems.Register<ECS::Sys_Door>            (270);

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

    // ── 6. Awake all systems ────────────────────────────────────────────
    systems.AwakeAll(registry);

    // ── 7. UI HUD + FadeIn ──────────────────────────────────────────────
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

    // ── 8. Save/load + inventory + equipment sync ───────────────────────
    if (ECS::HasSaveFile()) {
        ECS::LoadGame(registry, false);
        if (registry.has_ctx<ECS::Res_ItemInventory2>()) {
            registry.ctx<ECS::Res_ItemInventory2>().OnRoundStart();
        }
    }

#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>()
     && registry.has_ctx<ECS::Res_GameState>()
     && registry.has_ctx<ECS::Res_ItemInventory2>()) {
        auto& ui  = registry.ctx<ECS::Res_UIState>();
        auto& gs  = registry.ctx<ECS::Res_GameState>();
        auto& inv = registry.ctx<ECS::Res_ItemInventory2>();

        int gadgetIndices[5] = {};
        int gadgetCount = 0;
        int weaponIndices[5] = {};
        int weaponCount = 0;
        for (int i = 0; i < inv.kItemCount; ++i) {
            if (inv.slots[i].itemType == ECS::ItemType::Gadget) {
                if (gadgetCount < 5) gadgetIndices[gadgetCount++] = i;
            } else {
                if (weaponCount < 5) weaponIndices[weaponCount++] = i;
            }
        }

        for (int s = 0; s < 2; ++s) {
            int idx = ui.missionEquippedItems[s];
            if (idx >= 0 && idx < gadgetCount) {
                int invIdx = gadgetIndices[idx];
                auto& slot = inv.slots[invIdx];
                size_t len = strlen(slot.name);
                if (len > sizeof(gs.itemSlots[s].name) - 1)
                    len = sizeof(gs.itemSlots[s].name) - 1;
                memcpy(gs.itemSlots[s].name, slot.name, len);
                gs.itemSlots[s].name[len] = '\0';
                gs.itemSlots[s].itemId  = static_cast<uint8_t>(slot.itemId);
                gs.itemSlots[s].count   = slot.carriedCount;
                gs.itemSlots[s].cooldown = 0.0f;
            } else {
                gs.itemSlots[s] = {};
            }
        }

        for (int s = 0; s < 2; ++s) {
            int idx = ui.missionEquippedWeapons[s];
            if (idx >= 0 && idx < weaponCount) {
                int invIdx = weaponIndices[idx];
                auto& slot = inv.slots[invIdx];
                size_t len = strlen(slot.name);
                if (len > sizeof(gs.weaponSlots[s].name) - 1)
                    len = sizeof(gs.weaponSlots[s].name) - 1;
                memcpy(gs.weaponSlots[s].name, slot.name, len);
                gs.weaponSlots[s].name[len] = '\0';
                gs.weaponSlots[s].itemId  = static_cast<uint8_t>(slot.itemId);
                gs.weaponSlots[s].count   = slot.carriedCount;
                gs.weaponSlots[s].cooldown = 0.0f;
            } else {
                gs.weaponSlots[s] = {};
            }
        }

        LOG_INFO("[Scene_HangerA] Equipment synced from MissionSelect: items=["
                 << (int)ui.missionEquippedItems[0] << "," << (int)ui.missionEquippedItems[1]
                 << "] weapons=[" << (int)ui.missionEquippedWeapons[0] << ","
                 << (int)ui.missionEquippedWeapons[1] << "]");
    }
#endif

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
    if (registry.has_ctx<ECS::Res_AIConfig>())        registry.ctx_erase<ECS::Res_AIConfig>();
    if (registry.has_ctx<ECS::Res_ItemInventory2>())  registry.ctx_erase<ECS::Res_ItemInventory2>();
    if (registry.has_ctx<ECS::Res_RadarState>())      registry.ctx_erase<ECS::Res_RadarState>();
    if (registry.has_ctx<ECS::Res_GameState>())       registry.ctx_erase<ECS::Res_GameState>();
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_ToastState>())      registry.ctx_erase<ECS::Res_ToastState>();
    if (registry.has_ctx<ECS::Res_ChatState>())       registry.ctx_erase<ECS::Res_ChatState>();
    if (registry.has_ctx<ECS::Res_InventoryState>())  registry.ctx_erase<ECS::Res_InventoryState>();
    if (registry.has_ctx<ECS::Res_LobbyState>())      registry.ctx_erase<ECS::Res_LobbyState>();
    if (registry.has_ctx<ECS::Res_DialogueData>())    registry.ctx_erase<ECS::Res_DialogueData>();
#endif

    registry.Clear();

    LOG_INFO("[Scene_HangerA] OnExit complete. All systems destroyed.");
}
