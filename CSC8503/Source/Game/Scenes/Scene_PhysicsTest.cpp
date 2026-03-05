#include "Scene_PhysicsTest.h"

#include "Assets.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Components/Res_EnemyTestState.h"
#include "Game/Components/Res_CapsuleState.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_EnemyAI.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Raycast.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiEnemyAI.h"
#include "Game/Systems/Sys_ImGuiPhysicsTest.h"
#include "Game/Systems/Sys_ImGuiCapsuleGen.h"
#endif

// ============================================================
// OnEnter（场景加载阶段）
// ============================================================

void Scene_PhysicsTest::OnEnter(ECS::Registry&          registry,
                                ECS::SystemManager&     systems,
                                const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热：初始化 AssetManager，加载本场景所需 mesh ──────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");
    LOG_INFO("[Scene_PhysicsTest] cube mesh loaded, handle=" << cubeMesh);

    // 采用 feat 分支的逻辑加载真实的胶囊体 Mesh
    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.msh");
    LOG_INFO("[Scene_PhysicsTest] capsule mesh loaded, handle=" << capsuleMesh);

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
    //    Res_NCL_Pointers 由 SceneManager 构造时已预注册，此处无需重复。

    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    if (!registry.has_ctx<Res_TestState>()) {
        Res_TestState state;
        state.cubeMeshHandle = cubeMesh;
        state.capsuleMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_TestState>(std::move(state));
    } else {
        auto& state = registry.ctx<Res_TestState>();
        state.cubeMeshHandle = cubeMesh;
        state.capsuleMeshHandle = capsuleMesh;
    }

    // 敌人实体池状态（由 Sys_ImGuiPhysicsTest 读写）
    if (!registry.has_ctx<Res_EnemyTestState>()) {
        Res_EnemyTestState enemyState;
        enemyState.enemyMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_EnemyTestState>(std::move(enemyState));
    }

    // 胶囊生成状态（由 Sys_ImGuiCapsuleGen 读写）
    if (!registry.has_ctx<Res_CapsuleState>()) {
        Res_CapsuleState capsuleState;
        capsuleState.capsuleMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_CapsuleState>(std::move(capsuleState));
    }

    // ── 3. 初始实体生成：通过 PrefabFactory 创建静态地板 ─────────────────
    //    相机实体由 Sys_Camera::OnAwake 创建（符合系统职责）
    ECS::EntityID entity_floor_main = PrefabFactory::CreateFloor(registry, cubeMesh);
    LOG_INFO("[Scene_PhysicsTest] floor entity id=" << entity_floor_main);

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    //    执行顺序：Camera(50) → Physics(100) → EnemyAI(120) → Render(200)
    //            → ImGui(300) → CapsuleGen(301) → EnemyMonitor(310) → PhysicsTest(320) → Raycast(330)
    systems.Register<ECS::Sys_Camera>   ( 50);   // 相机实体创建 + WASD/鼠标 + NCL Bridge
    systems.Register<ECS::Sys_Physics>  (100);   // Jolt Body 创建 + 物理步进 + Transform 同步
    systems.Register<ECS::Sys_EnemyAI>  (120);   // 敌人感知检测 + 四状态切换（Safe/Caution/Alert/Hunt）
    systems.Register<ECS::Sys_Render>   (200);   // ECS 实体 → NCL 代理对象桥接
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>             (300);   // 菜单栏 + 性能窗口 + Cube 控制面板
    systems.Register<ECS::Sys_ImGuiCapsuleGen>   (301);   // 胶囊生成/删除控制面板 (Master分支功能)
    systems.Register<ECS::Sys_ImGuiEnemyAI>      (310);   // 通用敌人状态监控表格（场景无关）
    systems.Register<ECS::Sys_ImGuiPhysicsTest>  (320);   // PhysicsTest 场景敌人生成/删除控制面板 (Feat分支功能)
#endif
    systems.Register<ECS::Sys_Raycast>           (330);   // Raycast 独立测试窗口（按钮触发 + 可视化）

    // ── 5. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    LOG_INFO("[Scene_PhysicsTest] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

void Scene_PhysicsTest::OnExit(ECS::Registry&       registry,
                               ECS::SystemManager& systems)
{
    // 逆序停机：PhysicsTest(320) → EnemyMonitor(310) → CapsuleGen(301) → ImGui(300) 
    // → Render(200) → EnemyAI(120) → Physics(100) → Camera(50)
    systems.DestroyAll(registry);

    // TODO: registry.Clear() —— 回收所有活动实体 ID，保留内存容量（Capacity）。
    //    当前 Registry 尚未实现 Clear() 接口，待补充后启用：
    //    registry.Clear();
    //    规范要求（游戏开发.md §3.3.2）：OnExit 必须调用 registry.Clear()，
    //    防止上一关实体状态污染下一关。

    LOG_INFO("[Scene_PhysicsTest] OnExit complete. All systems destroyed.");
}
