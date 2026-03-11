/**
 * @file Scene_NavTest.cpp
 * @brief 导航测试场景生命周期实现（资源加载、实体生成、系统注册）。
 */
#include "Scene_NavTest.h"

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
#include "Game/Systems/Sys_ImGuiEntityDebug.h"
#include "Game/Systems/Sys_ImGuiNavTest.h"
#endif

// ============================================================
// OnEnter（场景加载阶段）
// ============================================================

/**
 * @brief 进入导航测试场景并初始化导航测试所需资源与系统。
 * @details 加载导航测试资源、重置场景级 context、创建基础测试实体，并注册导航、感知、渲染与调试系统。
 * @param registry 当前场景注册表
 * @param systems 当前场景系统管理器
 * @param nclPtrs NCL 桥接资源（当前函数未直接使用）
 */
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
        NCL::Assets::MESHDIR + "Capsule.msh");
    LOG_INFO("[Scene_NavTest] capsule mesh loaded, handle=" << capsuleMesh);

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
        navState.enemyMeshHandle  = capsuleMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    // ── 3. 初始实体生成：创建静态地板 ────────────────────────────────────
    ECS::EntityID entity_floor = PrefabFactory::CreateFloor(registry, cubeMesh);
    LOG_INFO("[Scene_NavTest] floor entity id=" << entity_floor);

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

    systems.Register<ECS::Sys_Render>   (200);   // ECS 实体 → NCL 代理对象桥接
    systems.Register<ECS::Sys_EnemyAI>  (250);   // 敌人感知检测 + 四状态切换（读取 C_D_AIPerception::is_spotted）

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>           (300);   // 菜单栏 + 性能窗口
    systems.Register<ECS::Sys_ImGuiEntityDebug>(305);   // 全量实体列表 + 详情面板
    systems.Register<ECS::Sys_ImGuiNavTest>    (310);   // NavTest 敌人/目标生成控制面板
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

    LOG_INFO("[Scene_NavTest] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

/**
 * @brief 退出导航测试场景并清理导航相关上下文与路径工具。
 * @details 逆序销毁当前场景系统，释放路径查询工具和场景级 context，随后清空 Registry 中的全部实体与组件。
 * @param registry 当前场景注册表
 * @param systems 当前场景系统管理器
 */
void Scene_NavTest::OnExit(ECS::Registry&      registry,
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

    LOG_INFO("[Scene_NavTest] OnExit complete. All systems destroyed.");
}
