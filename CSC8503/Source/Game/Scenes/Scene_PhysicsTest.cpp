/**
 * @file Scene_PhysicsTest.cpp
 * @brief 物理测试场景生命周期实现（资源加载、实体生成、系统注册）。
 */
#include "Scene_PhysicsTest.h"

#include <cstring>
#include "Assets.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Components/Res_EnemyTestState.h"
#include "Game/Components/Res_CapsuleState.h"
#include "Game/Components/Res_CQCConfig.h"
#include "Game/Components/Res_DeathConfig.h"
#include "Game/Components/Res_VisionConfig.h"
#include "Game/Components/Res_AIConfig.h"
#include "Game/Systems/Sys_Countdown.h"
#include "Game/Systems/Sys_DeathJudgment.h"
#include "Game/Systems/Sys_DeathEffect.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_Input.h"
#include "Game/Systems/Sys_InputDispatch.h"
#include "Game/Systems/Sys_PlayerDisguise.h"
#include "Game/Systems/Sys_PlayerStance.h"
#include "Game/Systems/Sys_StealthMetrics.h"
#include "Game/Systems/Sys_Movement.h"
#include "Game/Systems/Sys_PlayerCQC.h"
#include "Game/Systems/Sys_EnemyVision.h"
#include "Game/Systems/Sys_EnemyAI.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_PlayerCamera.h"
#include "Game/Systems/Sys_Raycast.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Systems/Sys_Item.h"
#include "Game/Systems/Sys_ItemEffects.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/SaveManager.h"
#include "Game/Utils/ItemEquipSync.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiEntityDebug.h"
#include "Game/Systems/Sys_ImGuiEnemyAI.h"
#include "Game/Systems/Sys_ImGuiPhysicsTest.h"
#include "Game/Systems/Sys_ImGuiCapsuleGen.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Systems/Sys_Chat.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/Components/Res_ItemInventory2.h"
#include "Game/Components/Res_RadarState.h"
#include "Game/UI/UI_Toast.h"
#include "Game/Systems/Sys_ImGuiRenderDebug.h"
#endif

// ============================================================
// OnEnter（场景加载阶段）
// ============================================================

/**
 * @brief 进入物理测试场景：初始化 AssetManager、注册 ctx 资源、生成实体、注册系统。
 * @details 加载测试网格、重置测试态 context、生成基础地板/玩家/边界墙，并注册物理测试场景所需的玩法、渲染和调试系统。
 * @param registry ECS 注册表
 * @param systems  系统管理器（注册并 Awake 各系统）
 * @param nclPtrs  NCL 核心指针（GameWorld/PhysicsSystem/Renderer，当前未直接使用）
 */
void Scene_PhysicsTest::OnEnter(ECS::Registry&          registry,
                                 ECS::SystemManager&     systems,
                                 const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热：初始化 AssetManager，加载本场景所需 mesh ──────────
    ECS::AssetManager::Instance().Init();

    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "cube.obj");
    LOG_INFO("[Scene_PhysicsTest] cube mesh loaded, handle=" << cubeMesh);

    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.obj");
    LOG_INFO("[Scene_PhysicsTest] capsule mesh loaded, handle=" << capsuleMesh);

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
    //    Res_NCL_Pointers 由 SceneManager 构造时已预注册，此处无需重复。

    registry.ctx_emplace<Res_UIFlags>();

    // Res_TestState：首次进入时创建，场景重入时重置运行时状态、保留 mesh handle
    if (!registry.has_ctx<Res_TestState>()) {
        Res_TestState state;
        state.cubeMeshHandle    = cubeMesh;
        state.capsuleMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_TestState>(std::move(state));
    } else {
        // Registry 复用场景重入：更新 mesh handle 并重置所有运行时列表与索引，
        // 防止上一次场景遗留的失效 EntityID 污染本次的面板显示与删除逻辑。
        auto& state = registry.ctx<Res_TestState>();
        state.cubeMeshHandle      = cubeMesh;
        state.capsuleMeshHandle   = capsuleMesh;
        state.cubeEntities.clear();
        state.capsuleEntities.clear();
        state.spawnIndex          = 0;
        state.capsuleSpawnIndex   = 0;
        state.capsuleOverlapSpawn = false;
    }

    // 敌人实体池状态（由 Sys_ImGuiPhysicsTest 读写）
    {
        Res_EnemyTestState enemyState;
        enemyState.enemyMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_EnemyTestState>(std::move(enemyState));
    }

    // 胶囊生成状态（由 Sys_ImGuiCapsuleGen 读写）
    {
        Res_CapsuleState capsuleState;
        capsuleState.capsuleMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_CapsuleState>(std::move(capsuleState));
    }

    // CQC 配置资源（数据驱动）
    if (!registry.has_ctx<ECS::Res_CQCConfig>()) {
        registry.ctx_emplace<ECS::Res_CQCConfig>(ECS::Res_CQCConfig{});
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

    if (!registry.has_ctx<ECS::Res_AIConfig>()) {
        registry.ctx_emplace<ECS::Res_AIConfig>(ECS::Res_AIConfig{});
    }

    // ── 3. 初始实体生成：通过 PrefabFactory 创建静态地板 + 玩家 ──────────
    //    相机实体由 Sys_Camera::OnAwake 创建（符合系统职责）
    ECS::EntityID entity_floor_main = PrefabFactory::CreateFloor(registry, cubeMesh);
    LOG_INFO("[Scene_PhysicsTest] floor entity id=" << entity_floor_main);

    // 玩家实体（地板 Y=-6，地板厚度=1，顶面 Y=-5；胶囊半高=1+半径0.5=1.5，底部中心需 Y=-3.5）
    ECS::EntityID entity_player = PrefabFactory::CreatePlayer(
        registry, cubeMesh, NCL::Maths::Vector3(0.0f, -3.5f, 0.0f));
    LOG_INFO("[Scene_PhysicsTest] player entity id=" << entity_player);

    // 隐形碰撞墙 — 地板四周边界（XZ=±50，墙厚 1，高 10，紧贴地板边缘不重叠）
    // +X 边界墙（右）
    PrefabFactory::CreateInvisibleWall(registry, 0,
        NCL::Maths::Vector3( 50.5f, 0.0f, 0.0f), NCL::Maths::Vector3(0.5f, 5.0f, 51.0f));
    // -X 边界墙（左）
    PrefabFactory::CreateInvisibleWall(registry, 1,
        NCL::Maths::Vector3(-50.5f, 0.0f, 0.0f), NCL::Maths::Vector3(0.5f, 5.0f, 51.0f));
    // +Z 边界墙（后）
    PrefabFactory::CreateInvisibleWall(registry, 2,
        NCL::Maths::Vector3(0.0f, 0.0f,  50.5f), NCL::Maths::Vector3(51.0f, 5.0f, 0.5f));
    // -Z 边界墙（前）
    PrefabFactory::CreateInvisibleWall(registry, 3,
        NCL::Maths::Vector3(0.0f, 0.0f, -50.5f), NCL::Maths::Vector3(51.0f, 5.0f, 0.5f));
    LOG_INFO("[Scene_PhysicsTest] 4 invisible boundary walls created");

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    //    执行顺序：Input(10) → InputDispatch(55)
    //              → Disguise(59) → Stance(60) → StealthMetrics(62)
    //              → PlayerCQC(63) → Movement(65) → Physics(100)
    //              → EnemyVision(110) → EnemyAI(120) → DeathJudgment(125) → DeathEffect(126)
    //              → PlayerCamera(150) → Camera(155, Bridge 同步 + debug 飞行)
    //              → Render(200) → ImGui(300+) → Raycast(330) → Chat(450) → UI(500)
    systems.Register<ECS::Sys_Input>           ( 10);   // NCL → Res_Input（via InputAdapter）
    systems.Register<ECS::Sys_InputDispatch>   ( 55);   // Res_Input → per-entity C_D_Input
    systems.Register<ECS::Sys_PlayerDisguise>  ( 59);   // 伪装切换、C_T_Hidden 管理
    systems.Register<ECS::Sys_PlayerStance>    ( 60);   // 蹲/站切换、碰撞体替换
    systems.Register<ECS::Sys_StealthMetrics>  ( 62);   // 奔跑、速度乘数、噪音、可见度
    systems.Register<ECS::Sys_PlayerCQC>       ( 63);   // CQC 近身制服 + 目标选择
    systems.Register<ECS::Sys_Movement>        ( 65);   // 物理移动
    systems.Register<ECS::Sys_Physics>         (100);   // Jolt Body 创建 + 物理步进 + Transform 同步
    systems.Register<ECS::Sys_EnemyVision>    (110);   // 敌人视野判定（扇形视锥 + 遮挡射线）
    systems.Register<ECS::Sys_EnemyAI>         (120);   // 敌人感知检测 + 四状态切换（Safe/Search/Alert/Hunt）
    systems.Register<ECS::Sys_DeathJudgment>   (125);   // 死亡判定（敌人抓捕 + HP归零 + 触发器即死 → 场景重启/敌人销毁）
    systems.Register<ECS::Sys_DeathEffect>     (126);   // 死亡视觉特效（赛博朋克四阶段：冲击→故障→溶解→崩塌）
    systems.Register<ECS::Sys_PlayerCamera>    (150);   // 第三人称跟随相机
    systems.Register<ECS::Sys_Camera>          (155);   // 相机实体创建 + NCL Bridge 同步 + debug 飞行
    systems.Register<ECS::Sys_Render>          (200);   // ECS 实体 → NCL 代理对象桥接
    systems.Register<ECS::Sys_Item>            (250);   // 道具管理（拾取/使用/库存结算）
    systems.Register<ECS::Sys_ItemEffects>     (260);   // 道具效果执行（HoloBait/DDoS/RoamAI/Radar/TargetStrike）
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>             (300);   // 菜单栏 + 性能窗口 + 测试控制面板
    systems.Register<ECS::Sys_ImGuiEntityDebug>  (305);   // 全量实体列表 + 详情面板
    systems.Register<ECS::Sys_ImGuiEnemyAI>      (310);   // 通用敌人状态监控表格（场景无关）
    systems.Register<ECS::Sys_ImGuiPhysicsTest>  (320);   // PhysicsTest 场景敌人生成/删除控制面板
    systems.Register<ECS::Sys_ImGuiRenderDebug>  (445);   // 渲染参数调试面板
    systems.Register<ECS::Sys_Chat>              (450);   // 对话逻辑（在 UI 之前）
    systems.Register<ECS::Sys_UI>                (500);   // UI 渲染 + 输入导航
#endif
    systems.Register<ECS::Sys_Raycast>           (330);   // Raycast 独立测试窗口（按钮触发 + 可视化）
    systems.Register<ECS::Sys_Countdown>          (350);   // alertLevel≥100 → 30s 倒计时 → GameOver

    // ── 5. 初始化游戏状态资源 ──────────────────────────────────────────────
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

        // 重置场景过渡锁（防止卡在 Loading screen）
        ui.sceneRequestDispatched = false;

        // 启动 FadeIn（CRT 展开）
        ui.transitionActive     = true;
        ui.transitionTimer      = 0.0f;
        ui.transitionDuration   = 0.5f;
        ui.transitionType       = 0;  // FadeIn

        // 初始化光标状态（游戏场景默认锁定相机）
        ui.gameCursorFree = false;  // 游戏模式，相机锁定
        ui.cursorVisible  = false;  // 游戏中隐藏光标
        ui.cursorLocked   = true;   // 锁定光标用于相机控制
    }

    ECS::UI::PushToast(registry, "MISSION START", ECS::ToastType::Success, 2.5f);
#endif

    // ── 8. 存档加载 + 库存恢复 + 装备同步 ──────────────────────────────
    // 顺序：LoadGame → 恢复 storeCount → OnRoundStart → 装备同步 → SaveGame
    if (ECS::HasSaveFile()) {
        ECS::LoadGame(registry, false);  // 恢复 storeCount，不覆盖用户刚选的 map
        if (registry.has_ctx<ECS::Res_ItemInventory2>()) {
            // 用正确的 storeCount 重新分配 carriedCount
            registry.ctx<ECS::Res_ItemInventory2>().OnRoundStart();
        }
    }

    // 装备同步：从 Res_UIState 读 MissionSelect 选择，写入 Res_GameState
    ECS::SyncEquipmentToGameState(registry);

    LOG_INFO("[Scene_PhysicsTest] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

/**
 * @brief 退出物理测试场景：逆序销毁所有系统，清除场景级 ctx 资源，防止跨场景状态泄漏。
 * @details 逆序销毁系统后移除测试场景注入的配置、UI 和测试状态资源，最后清空 Registry 中的全部实体与组件。
 * @param registry ECS 注册表
 * @param systems  系统管理器（调用 DestroyAll）
 */
void Scene_PhysicsTest::OnExit(ECS::Registry&       registry,
                                ECS::SystemManager& systems)
{
    // 逆序停机（Sys_Item::OnDestroy 调用 OnRoundEnd → carried→storeCount）
    systems.DestroyAll(registry);

    // 结算后保存（storeCount 已更新，Res_ItemInventory2 尚未擦除）
    ECS::SaveGame(registry);

    // 清除场景指针 ctx，防止 delete 后悬空指针
    if (registry.has_ctx<IScene*>()) {
        registry.ctx_erase<IScene*>();
    }

    // 清除场景级 ctx 资源，防止跨场景状态泄漏（registry.Clear() 不清除 ctx）
    // Session 级资源（Res_UIState）不在此清除 — 跨场景保持用户设置
    if (registry.has_ctx<Res_UIFlags>())              registry.ctx_erase<Res_UIFlags>();
    if (registry.has_ctx<ECS::Res_DeathConfig>())     registry.ctx_erase<ECS::Res_DeathConfig>();
    if (registry.has_ctx<ECS::Res_VisionConfig>())    registry.ctx_erase<ECS::Res_VisionConfig>();
    if (registry.has_ctx<ECS::Res_AIConfig>())       registry.ctx_erase<ECS::Res_AIConfig>();
    if (registry.has_ctx<ECS::Res_CQCConfig>())       registry.ctx_erase<ECS::Res_CQCConfig>();
    if (registry.has_ctx<Res_TestState>())      registry.ctx_erase<Res_TestState>();
    if (registry.has_ctx<Res_EnemyTestState>()) registry.ctx_erase<Res_EnemyTestState>();
    if (registry.has_ctx<Res_CapsuleState>())   registry.ctx_erase<Res_CapsuleState>();
    if (registry.has_ctx<ECS::Res_ItemInventory2>())   registry.ctx_erase<ECS::Res_ItemInventory2>();
    if (registry.has_ctx<ECS::Res_RadarState>())      registry.ctx_erase<ECS::Res_RadarState>();
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_GameState>())      registry.ctx_erase<ECS::Res_GameState>();
    if (registry.has_ctx<ECS::Res_ToastState>())     registry.ctx_erase<ECS::Res_ToastState>();
    // Res_ChatState preserved across maps (only erased in MainMenu)
    if (registry.has_ctx<ECS::Res_InventoryState>()) registry.ctx_erase<ECS::Res_InventoryState>();
    if (registry.has_ctx<ECS::Res_LobbyState>())     registry.ctx_erase<ECS::Res_LobbyState>();
    if (registry.has_ctx<ECS::Res_DialogueData>())   registry.ctx_erase<ECS::Res_DialogueData>();
#endif

    // 回收所有活动实体
    registry.Clear();

    LOG_INFO("[Scene_PhysicsTest] OnExit complete. All systems destroyed, entities cleared.");
}
