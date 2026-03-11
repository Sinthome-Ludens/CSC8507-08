/**
 * @file Scene_TutorialLevel.cpp
 * @brief Tutorial Level 场景生命周期实现（资源加载、实体生成、系统注册）。
 */
#include "Scene_TutorialLevel.h"

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

void Scene_TutorialLevel::OnEnter(ECS::Registry&          registry,
                            ECS::SystemManager&     systems,
                            const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热：初始化 AssetManager，加载本场景所需 mesh ──────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle mapMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "TutorialMap.obj");

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");
    LOG_INFO("[Scene_TutorialLevel] cube mesh loaded, handle=" << cubeMesh);

    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.msh");
    LOG_INFO("[Scene_TutorialLevel] capsule mesh loaded, handle=" << capsuleMesh);

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    // 死亡判定配置资源（数据驱动）
    if (!registry.has_ctx<ECS::Res_DeathConfig>()) {
        registry.ctx_emplace<ECS::Res_DeathConfig>(ECS::Res_DeathConfig{});
    }

    // 场景指针（供 Sys_DeathJudgment 调用 Restart）
    registry.ctx_emplace<IScene*>(static_cast<IScene*>(this));

    // 视野检测配置资源（数据驱动）
    if (!registry.has_ctx<ECS::Res_VisionConfig>()) {
        registry.ctx_emplace<ECS::Res_VisionConfig>(ECS::Res_VisionConfig{});
    }

    // 无条件重置：场景重进时 DestroyAll 已销毁旧实体，ctx 中残留的实体 ID 列表
    // 若不清空会导致 "Delete Last" 操作访问已失效 ID
    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;    // Cube：Capsule.msh 未实现，改用已正常加载的 cube.obj
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    // ── 3. 初始实体生成：创建 TutorialMap 地图实体 ───────────────────────
    ECS::EntityID entity_map = PrefabFactory::CreateStaticMap(registry, mapMesh);
    LOG_INFO("[Scene_TutorialLevel] map entity id=" << entity_map);

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    //    执行顺序：Camera(50) → Physics(100) → EnemyVision(110)
    //              → DeathJudgment(125) → Navigation(130) → Render(200)
    //              → EnemyAI(250) → ImGui(300) → NavTest(310)
    systems.Register<ECS::Sys_Camera>       ( 50);   // 相机实体创建 + WASD/鼠标 + NCL Bridge
    systems.Register<ECS::Sys_Physics>      (100);   // Jolt Body 创建 + 物理步进 + Transform 同步
    systems.Register<ECS::Sys_EnemyVision>  (110);   // 敌人视野判定（扇形视锥 + 遮挡射线）
    systems.Register<ECS::Sys_DeathJudgment>(125);   // 死亡判定（敌人抓捕 + HP归零 + 触发器即死）

    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());
    m_Pathfinder->LoadNavMesh(NCL::Assets::MESHDIR + "TutorialMap.navmesh");

    // ── 墙体碰撞体自动生成 ──────────────────────────────────────────────
    // 从 navmesh 边界边提取墙面位置，创建隐形 Box 碰撞体。
    //
    // 坐标系对齐：
    //   navmesh Y ≈ 0.583（Unity 原始坐标系）
    //   地图实体 Y 偏移 = -6.0（CreateStaticMap Transform）
    //   物理世界地面 Y = 0.583 + (-6.0) = -5.417
    //
    // 墙体参数：
    //   高度 8m（半高 4m），完全覆盖可通行区域
    //   厚度 0.5m（半厚 0.25m），防止隧穿
    {
        constexpr float kMapYOffset    = -6.0f;
        constexpr float kWallHalfH     = 4.0f;
        constexpr float kWallHalfThick = 0.25f;

        auto edges = m_Pathfinder->GetBoundaryEdges();
        int wallIdx = 0;

        for (const auto& edge : edges) {
            // 墙体中心 Y = 边在 navmesh 中的 Y + 地图偏移 + 半高
            float worldCenterY = edge.midpoint.y + kMapYOffset + kWallHalfH;

            NCL::Maths::Vector3 wallPos(
                edge.midpoint.x,
                worldCenterY,
                edge.midpoint.z);

            // 旋转：使 Box 局部 X 轴对齐边方向
            // Ry(θ) * (1,0,0) = (cosθ, 0, −sinθ) = (dirX, 0, dirZ)
            // → θ = atan2(−dirZ, dirX)
            float yawDeg = atan2f(-edge.dirZ, edge.dirX) * 57.29577f;
            NCL::Maths::Quaternion wallRot =
                NCL::Maths::Quaternion::EulerAnglesToQuaternion(0.0f, yawDeg, 0.0f);

            // Box 半尺寸：X 方向沿边，Y 方向为高，Z 方向为厚度
            NCL::Maths::Vector3 halfExtents(
                edge.length * 0.5f,
                kWallHalfH,
                kWallHalfThick);

            PrefabFactory::CreateInvisibleWall(
                registry, wallIdx++, wallPos, halfExtents, wallRot);
        }

        LOG_INFO("[Scene_TutorialLevel] Generated " << wallIdx
                 << " wall colliders from navmesh boundary edges.");
    }

    systems.Register<ECS::Sys_Render>   (200);   // ECS 实体 → NCL 代理对象桥接
    systems.Register<ECS::Sys_EnemyAI>  (250);   // 敌人感知检测 + 四状态切换（读取 C_D_AIPerception::is_spotted）

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>        (300);   // 菜单栏 + 性能窗口
    systems.Register<ECS::Sys_ImGuiNavTest> (310);   // NavTest 敌人/目标生成控制面板
#endif

    // ── 5. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    // 重置场景过渡状态和光标状态
    if (registry.has_ctx<ECS::Res_UIState>()) {
        auto& ui = registry.ctx<ECS::Res_UIState>();

        // 重置场景过渡锁（防止卡在 Loading screen）
        ui.sceneRequestDispatched = false;
        ui.transitionActive       = false;
        ui.transitionTimer        = 0.0f;

        // 初始化光标状态（NavTest 需要自由相机）
        ui.gameCursorFree = true;   // 测试场景，自由相机
        ui.cursorVisible  = true;   // 显示光标
        ui.cursorLocked   = false;  // 不锁定光标
    }

    LOG_INFO("[Scene_TutorialLevel] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

void Scene_TutorialLevel::OnExit(ECS::Registry&      registry,
                           ECS::SystemManager& systems)
{
    // 逆序停机
    systems.DestroyAll(registry);
    m_Pathfinder.reset();

    // 清除场景指针 ctx，防止 delete 后悬空指针
    if (registry.has_ctx<IScene*>()) {
        registry.ctx_erase<IScene*>();
    }

    // 清除场景级 ctx 资源，防止跨场景状态泄漏（registry.Clear() 不清除 ctx）
    if (registry.has_ctx<Res_UIFlags>())              registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<Res_NavTestState>())          registry.ctx_erase<Res_NavTestState>();
    if (registry.has_ctx<ECS::Res_DeathConfig>())     registry.ctx_erase<ECS::Res_DeathConfig>();
    if (registry.has_ctx<ECS::Res_VisionConfig>())    registry.ctx_erase<ECS::Res_VisionConfig>();

    registry.Clear();

    LOG_INFO("[Scene_TutorialLevel] OnExit complete. All systems destroyed.");
}
