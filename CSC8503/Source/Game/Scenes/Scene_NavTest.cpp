#include "Scene_NavTest.h"

#include "Assets.h"
#include "Core/Bridge/AssetManager.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_EnemyAI.h"
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

void Scene_NavTest::OnEnter(ECS::Registry&          registry,
                            ECS::SystemManager&     systems,
                            const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热：初始化 AssetManager，加载本场景所需 mesh ──────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");
    LOG_INFO("[Scene_NavTest] cube mesh loaded, handle=" << cubeMesh);

    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.obj");
    LOG_INFO("[Scene_NavTest] capsule mesh loaded, handle=" << capsuleMesh);

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    if (!registry.has_ctx<Res_NavTestState>()) {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = capsuleMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    // ── 3. 初始实体生成：创建静态地板 ────────────────────────────────────
    ECS::EntityID entity_floor = PrefabFactory::CreateFloor(registry, cubeMesh);
    LOG_INFO("[Scene_NavTest] floor entity id=" << entity_floor);

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    //    执行顺序：Camera(50) → Physics(100) → Navigation(130) → Render(200) → EnemyAI(250) → ImGui(300) → NavTest(310)
    systems.Register<ECS::Sys_Camera>   ( 50);   // 相机实体创建 + WASD/鼠标 + NCL Bridge
    systems.Register<ECS::Sys_Physics>  (100);   // Jolt Body 创建 + 物理步进 + Transform 同步

    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());

    systems.Register<ECS::Sys_Render>   (200);   // ECS 实体 → NCL 代理对象桥接
    systems.Register<ECS::Sys_EnemyAI>  (250);   // 敌人感知检测 + 四状态切换（读取 isSpotted）

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>        (300);   // 菜单栏 + 性能窗口
    systems.Register<ECS::Sys_ImGuiNavTest> (310);   // NavTest 敌人/目标生成控制面板
#endif

    // ── 5. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    LOG_INFO("[Scene_NavTest] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

void Scene_NavTest::OnExit(ECS::Registry&      registry,
                           ECS::SystemManager& systems)
{
    // 逆序停机：NavTest(310) → ImGui(300) → EnemyAI(250) → Render(200) → Navigation(130) → Physics(100) → Camera(50)
    systems.DestroyAll(registry);
    m_Pathfinder.reset();

    LOG_INFO("[Scene_NavTest] OnExit complete. All systems destroyed.");
}
