/**
 * @file Scene_Lab.cpp
 * @brief Lab 关卡场景生命周期实现（资源加载、实体生成、系统注册）。
 */
#include "Scene_Lab.h"

#include <cmath>
#include <cstring>
#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_VisionConfig.h"
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
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Components/C_T_FinishZone.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_Material.h"
#include "Core/Bridge/AssimpLoader.h"

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
// OnEnter（场景加载阶段）
// ============================================================
/**
 * @brief 加载地图资源、注册并唤醒所有系统、初始化碰撞体。
 * Y 偏移 -6*kMapScale 将 NavMesh 本地坐标对齐到世界渲染位置。
 */
void Scene_Lab::OnEnter(ECS::Registry&          registry,
                        ECS::SystemManager&     systems,
                        const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热：初始化 AssetManager，加载本场景所需 mesh ──────────
    ECS::AssetManager::Instance().Init();

    // 渲染用 collision OBJ（不含 SpawnPoint/PatrolPoint 标记物）
    ECS::MeshHandle mapMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Lab_collision.obj");

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");
    LOG_INFO("[Scene_Lab] cube mesh loaded, handle=" << cubeMesh);

    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.msh");
    LOG_INFO("[Scene_Lab] capsule mesh loaded, handle=" << capsuleMesh);

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
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

    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    // ── 3. 初始实体生成：创建 Lab 地图实体 ───────────────────────
    constexpr float kMapScale = 1.0f;

    ECS::EntityID entity_map = PrefabFactory::CreateStaticMapRenderOnly(registry, mapMesh, kMapScale);
    LOG_INFO("[Scene_Lab] map entity id=" << entity_map);

    // 碰撞体：优先加载 _collision.obj，不存在则 fallback 到渲染用 .obj
    {
        std::string collObjPath = NCL::Assets::MESHDIR + "Lab_collision.obj";
        std::vector<NCL::Maths::Vector3> mapCollVerts;
        std::vector<int>                 mapCollIndices;
        bool mapCollLoaded = ECS::AssimpLoader::LoadCollisionGeometry(
            collObjPath, mapCollVerts, mapCollIndices);

        if (!mapCollLoaded || mapCollVerts.empty()) {
            collObjPath = NCL::Assets::MESHDIR + "Lab.obj";
            mapCollLoaded = ECS::AssimpLoader::LoadCollisionGeometry(
                collObjPath, mapCollVerts, mapCollIndices);
            LOG_WARN("[Scene_Lab] _collision.obj not found, falling back to Lab.obj");
        }

        if (mapCollLoaded && !mapCollVerts.empty()) {
            for (auto& v : mapCollVerts) {
                v.x *= kMapScale;
                v.y *= kMapScale;
                v.z *= kMapScale;
            }
            for (size_t i = 0; i + 2 < mapCollIndices.size(); i += 3) {
                std::swap(mapCollIndices[i + 1], mapCollIndices[i + 2]);
            }
            PrefabFactory::CreateNavMeshFloor(registry, mapCollVerts, mapCollIndices,
                                              NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f));
            LOG_INFO("[Scene_Lab] Collision mesh loaded from " << collObjPath
                     << " (" << mapCollVerts.size() << " verts, " << mapCollIndices.size() / 3 << " tris)");
        } else {
            LOG_WARN("[Scene_Lab] Failed to load any collision geometry!");
        }
    }

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
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
    systems.Register<ECS::Sys_LevelGoal>       (127);
    systems.Register<ECS::Sys_DeathEffect>     (126);

    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());
    m_Pathfinder->LoadNavMesh(NCL::Assets::MESHDIR + "Lab.navmesh");
    m_Pathfinder->ScaleVertices(kMapScale);
    m_Pathfinder->OffsetVertices(NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f));

    // ── 终点区域生成 ────────────────────────────────────────────────────
    {
        ECS::MeshHandle finishMesh = ECS::AssetManager::Instance().LoadMesh(
            NCL::Assets::MESHDIR + "Lab_finish.obj");

        std::vector<NCL::Maths::Vector3> finVerts;
        std::vector<int> finIdx;
        ECS::AssimpLoader::LoadCollisionGeometry(
            NCL::Assets::MESHDIR + "Lab_finish.obj", finVerts, finIdx);

        NCL::Maths::Vector3 objCenter(0, 0, 0);
        if (!finVerts.empty()) {
            for (const auto& v : finVerts) {
                objCenter.x += v.x;
                objCenter.y += v.y;
                objCenter.z += v.z;
            }
            float n = static_cast<float>(finVerts.size());
            objCenter.x /= n;
            objCenter.y /= n;
            objCenter.z /= n;
        }

        NCL::Maths::Vector3 detectPos(
            objCenter.x * kMapScale,
            objCenter.y * kMapScale + (-6.0f * kMapScale),
            objCenter.z * kMapScale);

        if (finishMesh != 0) {
            ECS::EntityID finishRender = registry.Create();
            registry.Emplace<ECS::C_D_Transform>(finishRender,
                NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f),
                NCL::Maths::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                NCL::Maths::Vector3(kMapScale, kMapScale, kMapScale));
            registry.Emplace<ECS::C_D_MeshRenderer>(finishRender,
                finishMesh, static_cast<uint32_t>(0));
            ECS::C_D_Material mat{};
            mat.baseColour = NCL::Maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
            registry.Emplace<ECS::C_D_Material>(finishRender, mat);
        }

        {
            ECS::EntityID finishDetect = registry.Create();
            registry.Emplace<ECS::C_D_Transform>(finishDetect,
                detectPos,
                NCL::Maths::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                NCL::Maths::Vector3(1.0f, 1.0f, 1.0f));
            registry.Emplace<ECS::C_T_FinishZone>(finishDetect);

            LOG_INFO("[Scene_Lab] Finish zone: render at map origin, "
                     << "detect at (" << detectPos.x << "," << detectPos.y << "," << detectPos.z << ")");
        }
    }

    // ── 玩家生成（从 .startpoints 或 .points 文件读取起始点）─────────────
    {
        auto points = ECS::LoadMapPoints(NCL::Assets::MESHDIR + "Lab.startpoints");
        if (!points.loaded) {
            points = ECS::LoadMapPoints(NCL::Assets::MESHDIR + "Lab.points");
        }
        if (points.loaded && !points.startPoints.empty()) {
            const auto& sp = points.startPoints[0];
            NCL::Maths::Vector3 spawnPos(
                sp.x * kMapScale,
                sp.y * kMapScale + (-6.0f * kMapScale) + 1.5f,
                sp.z * kMapScale);
            ECS::EntityID player = PrefabFactory::CreatePlayer(registry, cubeMesh, spawnPos);
            registry.Emplace<ECS::C_T_NavTarget>(player);
        }
    }

    // ── 敌人生成（从 .enemyspawns 文件读取生成点与巡逻路线）──────────
    {
        auto enemyData = ECS::LoadEnemySpawns(NCL::Assets::MESHDIR + "Lab.enemyspawns");
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
            LOG_INFO("[Scene_Lab] Spawned " << enemyData.spawns.size()
                     << " enemies with patrol routes.");
        }
    }

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
    systems.Register<ECS::Sys_ImGuiRenderDebug>  (320);
    systems.Register<ECS::Sys_Chat>              (450);
    systems.Register<ECS::Sys_UI>                (500);
#endif
    systems.Register<ECS::Sys_Countdown>          (350);

    // ── 5. 初始化游戏状态资源 ────────────────────────────────────────────
    if (!registry.has_ctx<ECS::Res_GameState>()) {
        registry.ctx_emplace<ECS::Res_GameState>();
    }

    // ── 6. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    // ── 7. 设置 UI 为 HUD 模式并启动 FadeIn 过渡 ───────────────────────
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

    // ── 8. 存档加载 + 库存恢复 + 装备同步 ──────────────────────────────
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

        LOG_INFO("[Scene_Lab] Equipment synced from MissionSelect: items=["
                 << (int)ui.missionEquippedItems[0] << "," << (int)ui.missionEquippedItems[1]
                 << "] weapons=[" << (int)ui.missionEquippedWeapons[0] << ","
                 << (int)ui.missionEquippedWeapons[1] << "]");
    }
#endif

    LOG_INFO("[Scene_Lab] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================
/**
 * @brief 销毁所有系统、清理 NavMesh Pathfinder 及所有 context 资源。
 */
void Scene_Lab::OnExit(ECS::Registry&      registry,
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

    LOG_INFO("[Scene_Lab] OnExit complete. All systems destroyed.");
}
