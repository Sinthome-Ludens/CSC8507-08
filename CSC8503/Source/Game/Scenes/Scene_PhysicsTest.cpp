#include "Scene_PhysicsTest.h"

#include <cstdio>
#include "Assets.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameplayState.h"
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

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
    //    Res_NCL_Pointers 由 SceneManager 构造时已预注册，此处无需重复。

    // 始终用 ctx_emplace（覆盖写入），防止跨场景残留的过期 Context 导致崩溃。
    // SceneManager 在场景切换时调用 registry.Clear() 清空实体，但 Context 保留——
    // 因此必须在此处显式刷新场景级资源。
    registry.ctx_emplace<Res_UIFlags>();

    {
        Res_TestState state;
        state.cubeMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_TestState>(std::move(state));
    }

    // ── 游戏场景UI状态：覆盖写入，进入HUD画面 ──────────────────────
#ifdef USE_IMGUI
    {
        auto& uiState = registry.ctx_emplace<ECS::Res_UIState>();
        uiState.activeScreen = ECS::UIScreen::HUD;
        uiState.isUIBlockingInput = false;
    }

    // ── 游戏玩法状态：Phase 2 模拟数据驱动 HUD 开发 ─────────────────
    {
        auto& gs = registry.ctx_emplace<ECS::Res_GameplayState>();

        // 警戒度：初始安全
        gs.alertLevel = 0.0f;
        gs.alertMax   = 150.0f;

        // 倒计时：初始未启动
        gs.countdownTimer  = 120.0f;
        gs.countdownMax    = 120.0f;
        gs.countdownActive = false;

        // 玩家状态
        gs.playerMoveState = ECS::PlayerMoveState::Standing;
        gs.playerDisguised = false;

        // 任务信息
        snprintf(gs.missionName,   sizeof(gs.missionName),   "OPERATION GHOST");
        snprintf(gs.objectiveText, sizeof(gs.objectiveText), "Infiltrate server room B-7");

        // 道具槽模拟数据
        snprintf(gs.itemSlots[0].name, sizeof(gs.itemSlots[0].name), "EMP Grenade");
        gs.itemSlots[0].count    = 2;
        gs.itemSlots[0].cooldown = 0.0f;

        snprintf(gs.itemSlots[1].name, sizeof(gs.itemSlots[1].name), "Smoke Bomb");
        gs.itemSlots[1].count    = 1;
        gs.itemSlots[1].cooldown = 0.3f;

        // 武器槽模拟数据
        snprintf(gs.weaponSlots[0].name, sizeof(gs.weaponSlots[0].name), "Taser");
        gs.weaponSlots[0].count    = -1;  // 无限
        gs.weaponSlots[0].cooldown = 0.0f;

        snprintf(gs.weaponSlots[1].name, sizeof(gs.weaponSlots[1].name), "Hack Tool");
        gs.weaponSlots[1].count    = 3;
        gs.weaponSlots[1].cooldown = 0.0f;

        gs.activeItemSlot   = 0;
        gs.activeWeaponSlot = 0;
    }
#endif

    // ── 3. 初始实体生成：通过 PrefabFactory 创建静态地板 ─────────────────
    //    相机实体由 Sys_Camera::OnAwake 创建（符合系统职责）
    ECS::EntityID entity_floor_main = PrefabFactory::CreateFloor(registry, cubeMesh);
    LOG_INFO("[Scene_PhysicsTest] floor entity id=" << entity_floor_main);

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    //    执行顺序：Camera(50) → Physics(100) → Render(200) → ImGui(300)
    systems.Register<ECS::Sys_Camera>   ( 50);   // 相机实体创建 + WASD/鼠标 + NCL Bridge
    systems.Register<ECS::Sys_Physics>  (100);   // Jolt Body 创建 + 物理步进 + Transform 同步
    systems.Register<ECS::Sys_Render>   (200);   // ECS 实体 → NCL 代理对象桥接
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>    (300);   // 菜单栏 + 性能窗口 + TestScene 控制面板
    systems.Register<ECS::Sys_UI>       (500);   // 游戏UI系统
#endif

    // ── 5. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    LOG_INFO("[Scene_PhysicsTest] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

void Scene_PhysicsTest::OnExit(ECS::Registry&      registry,
                               ECS::SystemManager& systems)
{
    // 逆序停机：Sys_ImGui(300) → Sys_Render(200) → Sys_Physics(100) → Sys_Camera(50)
    systems.DestroyAll(registry);

    // registry.Clear() 由 SceneManager::EndFrame() 在 OnExit 之后统一调用，
    // 确保所有 System::OnDestroy 完成后再清空实体。

    LOG_INFO("[Scene_PhysicsTest] OnExit complete. All systems destroyed.");
}
