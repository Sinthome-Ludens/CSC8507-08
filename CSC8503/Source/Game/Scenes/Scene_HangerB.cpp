/**
 * @file Scene_HangerB.cpp
 * @brief HangerB 关卡场景生命周期实现（资源加载、实体生成、系统注册）。
 */
#include "Scene_HangerB.h"

#include <cmath>
#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Systems/Sys_DeathJudgment.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_EnemyAI.h"
#include "Game/Systems/Sys_EnemyVision.h"
#include "Game/Systems/Sys_Navigation.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiNavTest.h"
#endif

// ============================================================
// OnEnter（场景加载阶段）
// ============================================================

void Scene_HangerB::OnEnter(ECS::Registry&          registry,
                             ECS::SystemManager&     systems,
                             const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热 ─────────────────────────────────────────────────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle mapMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "HangerB.obj");

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");
    LOG_INFO("[Scene_HangerB] cube mesh loaded, handle=" << cubeMesh);

    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.msh");
    LOG_INFO("[Scene_HangerB] capsule mesh loaded, handle=" << capsuleMesh);

    // ── 2. 注册场景级全局资源 ───────────────────────────────────────────
    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
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

    // ── 3. 初始实体生成 ──────────────────────────────────────────────────
    constexpr float kMapScale = 1.0f;

    ECS::EntityID entity_map = PrefabFactory::CreateStaticMap(registry, mapMesh, kMapScale);
    LOG_INFO("[Scene_HangerB] map entity id=" << entity_map);

    // ── 4. 注册系统 ──────────────────────────────────────────────────────
    systems.Register<ECS::Sys_Camera>       ( 50);
    systems.Register<ECS::Sys_Physics>      (100);
    systems.Register<ECS::Sys_EnemyVision>  (110);
    systems.Register<ECS::Sys_DeathJudgment>(125);

    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());
    m_Pathfinder->LoadNavMesh(NCL::Assets::MESHDIR + "HangerB.navmesh");
    m_Pathfinder->ScaleVertices(kMapScale);

    // NavMesh 三角网格地板碰撞体（为斜坡/多层平台提供精确物理支撑）
    {
        std::vector<NCL::Maths::Vector3> floorVerts;
        std::vector<int>                 floorIndices;
        m_Pathfinder->GetWalkableGeometry(floorVerts, floorIndices);
        PrefabFactory::CreateNavMeshFloor(registry, floorVerts, floorIndices,
                                          NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f));
    }

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

        LOG_INFO("[Scene_HangerB] Generated " << wallIdx
                 << " wall colliders from navmesh boundary edges.");
    }

    systems.Register<ECS::Sys_Render>   (200);
    systems.Register<ECS::Sys_EnemyAI>  (250);

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>        (300);
    systems.Register<ECS::Sys_ImGuiNavTest> (310);
#endif

    // ── 5. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    if (registry.has_ctx<ECS::Res_UIState>()) {
        auto& ui = registry.ctx<ECS::Res_UIState>();
        ui.sceneRequestDispatched = false;
        ui.transitionActive       = false;
        ui.transitionTimer        = 0.0f;
        ui.gameCursorFree = true;
        ui.cursorVisible  = true;
        ui.cursorLocked   = false;
    }

    LOG_INFO("[Scene_HangerB] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

void Scene_HangerB::OnExit(ECS::Registry&      registry,
                            ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);
    m_Pathfinder.reset();

    if (registry.has_ctx<IScene*>()) {
        registry.ctx_erase<IScene*>();
    }

    if (registry.has_ctx<Res_UIFlags>())              registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<Res_NavTestState>())          registry.ctx_erase<Res_NavTestState>();
    if (registry.has_ctx<ECS::Res_DeathConfig>())     registry.ctx_erase<ECS::Res_DeathConfig>();
    if (registry.has_ctx<ECS::Res_VisionConfig>())    registry.ctx_erase<ECS::Res_VisionConfig>();

    registry.Clear();

    LOG_INFO("[Scene_HangerB] OnExit complete. All systems destroyed.");
}
