/**
 * @file Scene_CampaignLevel.cpp
 * @brief 战役关卡参数化场景：根据地图索引加载地图 + 注册完整玩法系统。
 */
#include "Scene_CampaignLevel.h"

#include <cmath>
#include <cstring>
#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_CampaignState.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Components/C_T_FinishZone.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_PatrolRoute.h"
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
#include "Game/Systems/Sys_LevelGoal.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/MapPointsLoader.h"
#include "Game/Utils/EnemySpawnLoader.h"
#include "Game/Utils/SaveManager.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiEntityDebug.h"
#include "Game/Systems/Sys_ImGuiEnemyAI.h"
#include "Game/Systems/Sys_ImGuiNavTest.h"
#include "Game/Systems/Sys_ImGuiRenderDebug.h"
#include "Game/Systems/Sys_Chat.h"
#include "Game/Systems/Sys_UI.h"
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
// Constructor
// ============================================================

Scene_CampaignLevel::Scene_CampaignLevel(int mapIndex)
    : m_MapIndex(mapIndex)
{
    if (m_MapIndex < 0 || m_MapIndex >= ECS::kCampaignMapCount) {
        LOG_WARN("[Scene_CampaignLevel] Invalid mapIndex " << m_MapIndex << ", clamping to 0");
        m_MapIndex = 0;
    }
}

// ============================================================
// OnEnter
// ============================================================

void Scene_CampaignLevel::OnEnter(ECS::Registry&          registry,
                                   ECS::SystemManager&     systems,
                                   const Res_NCL_Pointers& /*nclPtrs*/)
{
    const auto& mapDef = ECS::kCampaignMaps[m_MapIndex];
    LOG_INFO("[Scene_CampaignLevel] Loading map " << m_MapIndex
             << " (" << mapDef.displayName << ")");

    // ── 1. Asset loading ──────────────────────────────────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle mapMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + mapDef.meshFile);

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");

    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.msh");

    // ── 2. Context resources ──────────────────────────────────
    if (!registry.has_ctx<Res_UIFlags>())
        registry.ctx_emplace<Res_UIFlags>();

    if (!registry.has_ctx<ECS::Res_CQCConfig>())
        registry.ctx_emplace<ECS::Res_CQCConfig>(ECS::Res_CQCConfig{});

    if (!registry.has_ctx<ECS::Res_DeathConfig>())
        registry.ctx_emplace<ECS::Res_DeathConfig>(ECS::Res_DeathConfig{});

    registry.ctx_emplace<IScene*>(static_cast<IScene*>(this));

    if (!registry.has_ctx<ECS::Res_VisionConfig>())
        registry.ctx_emplace<ECS::Res_VisionConfig>(ECS::Res_VisionConfig{});

    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    // ── 3. Map entity ─────────────────────────────────────────
    constexpr float kMapScale = 1.0f;

    ECS::EntityID entity_map = PrefabFactory::CreateStaticMap(registry, mapMesh, kMapScale);
    LOG_INFO("[Scene_CampaignLevel] map entity id=" << entity_map);

    // ── 4. Systems registration ───────────────────────────────
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

    // ── 5. NavMesh ────────────────────────────────────────────
    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());
    m_Pathfinder->LoadNavMesh(NCL::Assets::MESHDIR + mapDef.navMeshFile);
    m_Pathfinder->ScaleVertices(kMapScale);

    // NavMesh floor collider
    {
        std::vector<NCL::Maths::Vector3> floorVerts;
        std::vector<int>                 floorIndices;
        m_Pathfinder->GetWalkableGeometry(floorVerts, floorIndices);
        PrefabFactory::CreateNavMeshFloor(registry, floorVerts, floorIndices,
                                          NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f));
    }

    // Boundary wall colliders
    {
        constexpr float kMapYOffset    = -6.0f  * kMapScale;
        constexpr float kWallHalfH     =  4.0f  * kMapScale;
        constexpr float kWallHalfThick =  0.25f * kMapScale;

        auto edges = m_Pathfinder->GetBoundaryEdges();
        int wallIdx = 0;

        for (const auto& edge : edges) {
            float worldCenterY = edge.midpoint.y + kMapYOffset + kWallHalfH;

            NCL::Maths::Vector3 wallPos(
                edge.midpoint.x,
                worldCenterY,
                edge.midpoint.z);

            float yawDeg = atan2f(-edge.dirZ, edge.dirX) * 57.29577f;
            NCL::Maths::Quaternion wallRot =
                NCL::Maths::Quaternion::EulerAnglesToQuaternion(0.0f, yawDeg, 0.0f);

            NCL::Maths::Vector3 halfExtents(
                edge.length * 0.5f,
                kWallHalfH,
                kWallHalfThick);

            PrefabFactory::CreateInvisibleWall(
                registry, wallIdx++, wallPos, halfExtents, wallRot);
        }

        LOG_INFO("[Scene_CampaignLevel] Generated " << wallIdx
                 << " wall colliders from navmesh boundary edges.");
    }

    // ── 6. Spawn point + finish zone ──────────────────────────
    NCL::Maths::Vector3 spawnPos(0.0f, 2.0f, 0.0f);  // fallback
    {
        auto points = ECS::LoadMapPoints(NCL::Assets::MESHDIR + mapDef.pointsFile);

        // Spawn: from .points or NavMesh first walkable vertex
        if (points.loaded && !points.startPoints.empty()) {
            const auto& sp = points.startPoints[0];
            spawnPos = NCL::Maths::Vector3(
                sp.x * kMapScale,
                sp.y * kMapScale + (-6.0f * kMapScale) + 1.5f,
                sp.z * kMapScale);
        } else if (m_Pathfinder->GetVertexCount() > 0) {
            // Fallback: first walkable vertex from NavMesh
            std::vector<NCL::Maths::Vector3> walkVerts;
            std::vector<int> walkIdx;
            m_Pathfinder->GetWalkableGeometry(walkVerts, walkIdx);
            if (!walkVerts.empty()) {
                spawnPos = NCL::Maths::Vector3(
                    walkVerts[0].x,
                    walkVerts[0].y + (-6.0f * kMapScale) + 1.5f,
                    walkVerts[0].z);
            }
            LOG_WARN("[Scene_CampaignLevel] No .points file, using NavMesh fallback spawn");
        }

        ECS::EntityID player = PrefabFactory::CreatePlayer(registry, cubeMesh, spawnPos);
        registry.Emplace<ECS::C_T_NavTarget>(player);
        LOG_INFO("[Scene_CampaignLevel] Player spawned at ("
                 << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

        // Finish zone: from .points finishPoints or NavMesh farthest vertex
        NCL::Maths::Vector3 finishPos(0.0f, 0.0f, 0.0f);
        bool hasFinish = false;

        if (points.loaded && !points.finishPoints.empty()) {
            const auto& fp = points.finishPoints[0];
            finishPos = NCL::Maths::Vector3(
                fp.x * kMapScale,
                fp.y * kMapScale + (-6.0f * kMapScale),
                fp.z * kMapScale);
            hasFinish = true;
        } else if (m_Pathfinder->GetVertexCount() > 0) {
            // Fallback: farthest walkable vertex from spawn
            std::vector<NCL::Maths::Vector3> walkVerts;
            std::vector<int> walkIdx;
            m_Pathfinder->GetWalkableGeometry(walkVerts, walkIdx);
            float maxDistSq = 0.0f;
            for (const auto& v : walkVerts) {
                float worldY = v.y + (-6.0f * kMapScale);
                float dx = v.x - spawnPos.x;
                float dz = v.z - spawnPos.z;
                float distSq = dx * dx + dz * dz;
                if (distSq > maxDistSq) {
                    maxDistSq = distSq;
                    finishPos = NCL::Maths::Vector3(v.x, worldY, v.z);
                }
            }
            hasFinish = true;
            LOG_WARN("[Scene_CampaignLevel] No finish in .points, using NavMesh farthest vertex");
        }

        if (hasFinish) {
            ECS::EntityID finishDetect = registry.Create();
            registry.Emplace<ECS::C_D_Transform>(finishDetect,
                finishPos,
                NCL::Maths::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                NCL::Maths::Vector3(1.0f, 1.0f, 1.0f));
            registry.Emplace<ECS::C_T_FinishZone>(finishDetect);
            LOG_INFO("[Scene_CampaignLevel] Finish zone at ("
                     << finishPos.x << "," << finishPos.y << "," << finishPos.z << ")");
        }
    }

    // ── 7. Enemy spawns (optional .enemyspawns file) ──────────
    {
        // Derive enemyspawns filename from mesh filename (e.g. "Dock.obj" -> "Dock.enemyspawns")
        std::string meshName = mapDef.meshFile;
        std::string baseName = meshName.substr(0, meshName.rfind('.'));
        auto enemyData = ECS::LoadEnemySpawns(NCL::Assets::MESHDIR + baseName + ".enemyspawns");
        if (enemyData.loaded) {
            for (int i = 0; i < static_cast<int>(enemyData.spawns.size()); ++i) {
                const auto& spawn = enemyData.spawns[i];

                NCL::Maths::Vector3 enemyPos(
                    spawn.position.x * kMapScale,
                    spawn.position.y * kMapScale + (-6.0f * kMapScale) + 1.5f,
                    spawn.position.z * kMapScale);

                ECS::EntityID enemy = PrefabFactory::CreateNavEnemy(
                    registry, cubeMesh, i, enemyPos);

                if (spawn.patrolPoints.size() >= 2) {
                    auto& patrol = registry.Emplace<ECS::C_D_PatrolRoute>(enemy);
                    patrol.count = std::min(
                        static_cast<int>(spawn.patrolPoints.size()),
                        ECS::PATROL_MAX_WAYPOINTS);
                    for (int p = 0; p < patrol.count; ++p) {
                        const auto& pt = spawn.patrolPoints[p];
                        patrol.waypoints[p] = NCL::Maths::Vector3(
                            pt.x * kMapScale,
                            pt.y * kMapScale + (-6.0f * kMapScale) + 1.5f,
                            pt.z * kMapScale);
                    }
                    patrol.current_index = 0;
                    patrol.needs_path    = true;

                    if (patrol.count >= 2) {
                        NCL::Maths::Vector3 dir = patrol.waypoints[1] - enemyPos;
                        dir.y = 0.0f;
                        float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
                        if (len > 0.01f) {
                            float yaw = atan2f(-dir.x / len, -dir.z / len) * 57.29577f;
                            auto& tf = registry.Get<ECS::C_D_Transform>(enemy);
                            tf.rotation = NCL::Maths::Quaternion::EulerAnglesToQuaternion(0, yaw, 0);
                        }
                    }
                }
            }
            LOG_INFO("[Scene_CampaignLevel] Spawned " << enemyData.spawns.size()
                     << " enemies with patrol routes.");
        }
    }

    // ── 8. Remaining systems ──────────────────────────────────
    systems.Register<ECS::Sys_PlayerCamera>    (150);
    systems.Register<ECS::Sys_Camera>          (155);
    systems.Register<ECS::Sys_Render>          (200);
    systems.Register<ECS::Sys_Item>            (250);
    systems.Register<ECS::Sys_ItemEffects>     (260);

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

    // ── 9. Game state ─────────────────────────────────────────
    if (!registry.has_ctx<ECS::Res_GameState>()) {
        registry.ctx_emplace<ECS::Res_GameState>();
    } else {
        // Reset game state for new round
        auto& gs = registry.ctx<ECS::Res_GameState>();
        gs.isGameOver      = false;
        gs.gameOverReason  = 0;
        gs.isPaused        = false;
        gs.playTime        = 0.0f;
        gs.alertLevel      = 0.0f;
        gs.countdownActive = false;
    }

    // ── 10. Awake all systems ─────────────────────────────────
    systems.AwakeAll(registry);

    // ── 11. UI setup ──────────────────────────────────────────
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
        ui.transitionType       = 0;  // FadeIn

        ui.gameCursorFree = false;
        ui.cursorVisible  = false;
        ui.cursorLocked   = true;
    }

    // Campaign round toast
    if (registry.has_ctx<ECS::Res_CampaignState>()) {
        auto& campaign = registry.ctx<ECS::Res_CampaignState>();
        if (campaign.active) {
            char msg[64];
            snprintf(msg, sizeof(msg), "MAP %d/%d: %s",
                     campaign.currentRound + 1, ECS::kCampaignRounds,
                     mapDef.displayName);
            ECS::UI::PushToast(registry, msg, ECS::ToastType::Info, 3.0f);
        }
    } else {
        ECS::UI::PushToast(registry, "MISSION START", ECS::ToastType::Success, 2.5f);
    }
#endif

    // ── 12. Save load + equipment sync ────────────────────────
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
    }
#endif

    LOG_INFO("[Scene_CampaignLevel] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit
// ============================================================

void Scene_CampaignLevel::OnExit(ECS::Registry&      registry,
                                  ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);

    ECS::SaveGame(registry);

    m_Pathfinder.reset();

    if (registry.has_ctx<IScene*>())
        registry.ctx_erase<IScene*>();

    // Clear scene-level ctx (NOT Res_UIState / Res_CampaignState — those persist)
    if (registry.has_ctx<Res_UIFlags>())              registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<Res_NavTestState>())          registry.ctx_erase<Res_NavTestState>();
    if (registry.has_ctx<ECS::Res_CQCConfig>())       registry.ctx_erase<ECS::Res_CQCConfig>();
    if (registry.has_ctx<ECS::Res_DeathConfig>())     registry.ctx_erase<ECS::Res_DeathConfig>();
    if (registry.has_ctx<ECS::Res_VisionConfig>())    registry.ctx_erase<ECS::Res_VisionConfig>();
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

    LOG_INFO("[Scene_CampaignLevel] OnExit complete. All systems destroyed.");
}
