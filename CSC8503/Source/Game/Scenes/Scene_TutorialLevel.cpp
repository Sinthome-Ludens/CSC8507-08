/**
 * @file Scene_TutorialLevel.cpp
 * @brief Tutorial Level 场景生命周期实现（资源加载、实体生成、系统注册）。
 */
#include "Scene_TutorialLevel.h"

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
#include "Game/Systems/Sys_ImGuiRenderDebug.h"
#endif

// ============================================================
// OnEnter（场景加载阶段）
// ============================================================
/**
 * @brief 加载地图资源、注册并唤醒所有系统、初始化 NavMesh 地板与边界墙碰撞体。
 * Y 偏移 -6*kMapScale 将 NavMesh 本地坐标对齐到世界渲染位置。
 */
void Scene_TutorialLevel::OnEnter(ECS::Registry&          registry,
                            ECS::SystemManager&     systems,
                            const Res_NCL_Pointers& /*nclPtrs*/)
{
    // ── 1. 资源预热：初始化 AssetManager，加载本场景所需 mesh ──────────
    ECS::AssetManager::Instance().Init();

    // 渲染用 collision OBJ（不含 SpawnPoint/PatrolPoint 标记物）
    // 原始 TutorialMap.obj 包含 Unity 导出的标记物几何，collision 版本已排除
    ECS::MeshHandle mapMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "TutorialMap_collision.obj");

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

    // 无条件重置：场景重进时 DestroyAll 已销毁旧实体，ctx 中残留的实体 ID 列表
    // 若不清空会导致 "Delete Last" 操作访问已失效 ID
    {
        Res_NavTestState navState;
        navState.enemyMeshHandle  = cubeMesh;
        navState.targetMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(navState));
    }

    // ── 3. 初始实体生成：创建 TutorialMap 地图实体 ───────────────────────
    // 缩放系数：修改此值可等比例缩放整个 TutorialLevel（视觉 + 物理 + 寻路同步）
    constexpr float kMapScale = 2.0f;

    // 渲染实体（纯渲染，不携带碰撞体）
    ECS::EntityID entity_map = PrefabFactory::CreateStaticMapRenderOnly(registry, mapMesh, kMapScale);
    LOG_INFO("[Scene_TutorialLevel] map entity id=" << entity_map);

    // 碰撞体：优先加载 _collision.obj（碰撞专用，不含窗口/玻璃等装饰面），
    //         不存在则 fallback 到渲染用 .obj
    {
        std::string collObjPath = NCL::Assets::MESHDIR + "TutorialMap_collision.obj";
        std::vector<NCL::Maths::Vector3> mapCollVerts;
        std::vector<int>                 mapCollIndices;
        bool mapCollLoaded = ECS::AssimpLoader::LoadCollisionGeometry(
            collObjPath, mapCollVerts, mapCollIndices);

        if (!mapCollLoaded || mapCollVerts.empty()) {
            collObjPath = NCL::Assets::MESHDIR + "TutorialMap.obj";
            mapCollLoaded = ECS::AssimpLoader::LoadCollisionGeometry(
                collObjPath, mapCollVerts, mapCollIndices);
            LOG_WARN("[Scene_TutorialLevel] _collision.obj not found, falling back to TutorialMap.obj");
        }

        if (mapCollLoaded && !mapCollVerts.empty()) {
            // OBJ 顶点为本地空间，需乘以 kMapScale 与渲染 Transform.scale 对齐
            for (auto& v : mapCollVerts) {
                v.x *= kMapScale;
                v.y *= kMapScale;
                v.z *= kMapScale;
            }
            // OBJ 由 Unity 导出脚本生成，导出时已同步完成左手系→右手系转换（Z 取反 + 绕序翻转）
            // 法线已朝外，无需额外处理绕序
            PrefabFactory::CreateNavMeshFloor(registry, mapCollVerts, mapCollIndices,
                                              NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f));
            LOG_INFO("[Scene_TutorialLevel] Collision mesh loaded from " << collObjPath
                     << " (" << mapCollVerts.size() << " verts, " << mapCollIndices.size() / 3 << " tris)");
        } else {
            LOG_WARN("[Scene_TutorialLevel] Failed to load any collision geometry!");
        }
    }

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    //    执行顺序：Input(10) → InputDispatch(55)
    //              → Disguise(59) → Stance(60) → StealthMetrics(62)
    //              → PlayerCQC(63) → Movement(65) → Physics(100)
    //              → EnemyVision(110) → EnemyAI(120) → DeathJudgment(125) → DeathEffect(126)
    //              → Navigation(130) → PlayerCamera(150) → Camera(155)
    //              → Render(200) → Item(250) → ItemEffects(260)
    //              → ImGui(300+) → Countdown(350) → Chat(450) → UI(500)
    systems.Register<ECS::Sys_Input>           ( 10);   // NCL → Res_Input
    systems.Register<ECS::Sys_InputDispatch>   ( 55);   // Res_Input → per-entity C_D_Input
    systems.Register<ECS::Sys_PlayerDisguise>  ( 59);   // 伪装切换、C_T_Hidden 管理
    systems.Register<ECS::Sys_PlayerStance>    ( 60);   // 蹲/站切换、碰撞体替换
    systems.Register<ECS::Sys_StealthMetrics>  ( 62);   // 奔跑、速度乘数、噪音、可见度
    systems.Register<ECS::Sys_PlayerCQC>       ( 63);   // CQC 近身制服 + 拟态
    systems.Register<ECS::Sys_Movement>        ( 65);   // 物理移动
    systems.Register<ECS::Sys_Physics>         (100);   // Jolt Body 创建 + 物理步进 + Transform 同步
    systems.Register<ECS::Sys_EnemyVision>     (110);   // 敌人视野判定（扇形视锥 + 遮挡射线）
    systems.Register<ECS::Sys_EnemyAI>         (120);   // 敌人感知检测 + 四状态切换（Safe/Search/Alert/Hunt）
    systems.Register<ECS::Sys_DeathJudgment>   (125);   // 死亡判定（敌人抓捕 + HP归零 + 触发器即死）
    systems.Register<ECS::Sys_LevelGoal>       (127);   // 关卡目标（玩家进入终点 → 过关）
    systems.Register<ECS::Sys_DeathEffect>     (126);   // 死亡视觉特效（四阶段动画）

    auto* navSys = systems.Register<ECS::Sys_Navigation>(130);
    m_Pathfinder = std::make_unique<ECS::NavMeshPathfinderUtil>();
    navSys->SetPathfinder(m_Pathfinder.get());
    m_Pathfinder->LoadNavMesh(NCL::Assets::MESHDIR + "TutorialMap.navmesh");
    m_Pathfinder->ScaleVertices(kMapScale);   // 寻路坐标与物理世界同步缩放
    m_Pathfinder->OffsetVertices(NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f));  // 局部→世界

    // 墙体碰撞由 TutorialMap_collision.obj 的 TriMesh 提供，
    // 不再从 NavMesh 边界边生成隐形墙（NavMesh 不经过窗口/开口，
    // 会在开口处产生边界边并用 Box 碰撞体封堵通道）。

    // ── 终点区域生成 ────────────────────────────────────────────────────
    // 渲染实体：TutorialMap_finish.obj 放在地图原点（OBJ 顶点自带位置）
    // 同一实体挂 C_T_FinishZone，Sys_LevelGoal 用 OBJ 几何中心做距离检测
    {
        ECS::MeshHandle finishMesh = ECS::AssetManager::Instance().LoadMesh(
            NCL::Assets::MESHDIR + "TutorialMap_finish.obj");

        // 从 OBJ 提取几何中心，用于放置检测点
        std::vector<NCL::Maths::Vector3> finVerts;
        std::vector<int> finIdx;
        ECS::AssimpLoader::LoadCollisionGeometry(
            NCL::Assets::MESHDIR + "TutorialMap_finish.obj", finVerts, finIdx);

        // 计算 OBJ 几何中心（本地空间）
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

        // 检测点世界坐标（OBJ 中心 * kMapScale + 地图偏移）
        NCL::Maths::Vector3 detectPos(
            objCenter.x * kMapScale,
            objCenter.y * kMapScale + (-6.0f * kMapScale),
            objCenter.z * kMapScale);

        // 1. 渲染实体（地图原点 + 缩放，OBJ 顶点自带位置）
        if (finishMesh != 0) {
            ECS::EntityID finishRender = registry.Create();
            registry.Emplace<ECS::C_D_Transform>(finishRender,
                NCL::Maths::Vector3(0.0f, -6.0f * kMapScale, 0.0f),
                NCL::Maths::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                NCL::Maths::Vector3(kMapScale, kMapScale, kMapScale));
            registry.Emplace<ECS::C_D_MeshRenderer>(finishRender,
                finishMesh, static_cast<uint32_t>(0));
            ECS::C_D_Material mat{};
            mat.baseColour = NCL::Maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f);  // 红色（匹配 mtl Kd）
            registry.Emplace<ECS::C_D_Material>(finishRender, mat);
        }

        // 2. 检测实体（不可见，放在 OBJ 几何中心的世界坐标）
        {
            ECS::EntityID finishDetect = registry.Create();
            registry.Emplace<ECS::C_D_Transform>(finishDetect,
                detectPos,
                NCL::Maths::Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
                NCL::Maths::Vector3(1.0f, 1.0f, 1.0f));
            registry.Emplace<ECS::C_T_FinishZone>(finishDetect);

            LOG_INFO("[Scene_TutorialLevel] Finish zone: render at map origin, "
                     << "detect at (" << detectPos.x << "," << detectPos.y << "," << detectPos.z << ")");
        }
    }

    // ── 玩家生成（从 .points 或 .startpoints 文件读取起始点）─────────────
    {
        // 优先加载 .points（包含 start + finish），若不存在则尝试 .startpoints
        auto points = ECS::LoadMapPoints(NCL::Assets::MESHDIR + "TutorialMap.points");
        if (!points.loaded) {
            points = ECS::LoadMapPoints(NCL::Assets::MESHDIR + "TutorialMap.startpoints");
        }
        if (points.loaded && !points.startPoints.empty()) {
            const auto& sp = points.startPoints[0];
            NCL::Maths::Vector3 spawnPos(
                sp.x * kMapScale,
                sp.y * kMapScale + (-6.0f * kMapScale) + 1.5f,
                sp.z * kMapScale);
            ECS::EntityID player = PrefabFactory::CreatePlayer(registry, capsuleMesh, spawnPos);
            // 挂载导航目标标签，使 Sys_Navigation 能定位玩家
            registry.Emplace<ECS::C_T_NavTarget>(player);
        }
    }

    // ── 敌人生成（从 .enemyspawns 文件读取生成点与巡逻路线）──────────
    {
        auto enemyData = ECS::LoadEnemySpawns(NCL::Assets::MESHDIR + "TutorialMap.enemyspawns");
        if (enemyData.loaded) {
            for (int i = 0; i < static_cast<int>(enemyData.spawns.size()); ++i) {
                const auto& spawn = enemyData.spawns[i];

                // 坐标变换：本地空间 → 世界空间（与玩家生成同一变换）
                NCL::Maths::Vector3 enemyPos(
                    spawn.position.x * kMapScale,
                    spawn.position.y * kMapScale + (-6.0f * kMapScale) + 1.5f,
                    spawn.position.z * kMapScale);

                ECS::EntityID enemy = PrefabFactory::CreateNavEnemy(
                    registry, cubeMesh, i, enemyPos);

                // 挂载巡逻路线组件
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

                    // 初始朝向：面向第二个巡逻路点（避免面墙）
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
            LOG_INFO("[Scene_TutorialLevel] Spawned " << enemyData.spawns.size()
                     << " enemies with patrol routes.");
        }
    }

    systems.Register<ECS::Sys_PlayerCamera>    (150);   // 第三人称跟随相机
    systems.Register<ECS::Sys_Camera>          (155);   // 相机实体创建 + NCL Bridge 同步 + debug 飞行
    systems.Register<ECS::Sys_Render>          (200);   // ECS 实体 → NCL 代理对象桥接
    systems.Register<ECS::Sys_Item>            (250);   // 道具管理（拾取/使用/库存结算）
    systems.Register<ECS::Sys_ItemEffects>     (260);   // 道具效果执行（HoloBait/DDoS/RoamAI/Radar/TargetStrike）

#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>             (300);   // 菜单栏 + 性能窗口
    systems.Register<ECS::Sys_ImGuiEntityDebug>  (305);   // 全量实体列表 + 详情面板
    systems.Register<ECS::Sys_ImGuiEnemyAI>      (310);   // 敌人状态监控表格
    systems.Register<ECS::Sys_ImGuiNavTest>      (315);   // NavTest 敌人/目标生成控制面板
    systems.Register<ECS::Sys_ImGuiRenderDebug>  (420);   // 渲染参数调试面板
    systems.Register<ECS::Sys_Chat>              (450);   // 对话逻辑（在 UI 之前）
    systems.Register<ECS::Sys_UI>                (500);   // UI 渲染 + 输入导航
#endif
    systems.Register<ECS::Sys_Countdown>          (350);   // alertLevel≥100 → 30s 倒计时 → GameOver

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

        LOG_INFO("[Scene_TutorialLevel] Equipment synced from MissionSelect: items=["
                 << (int)ui.missionEquippedItems[0] << "," << (int)ui.missionEquippedItems[1]
                 << "] weapons=[" << (int)ui.missionEquippedWeapons[0] << ","
                 << (int)ui.missionEquippedWeapons[1] << "]");
    }
#endif

    LOG_INFO("[Scene_TutorialLevel] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================
/**
 * @brief 销毁所有系统、清理 NavMesh Pathfinder 及所有 context 资源。
 */
void Scene_TutorialLevel::OnExit(ECS::Registry&      registry,
                           ECS::SystemManager& systems)
{
    // 逆序停机（Sys_Item::OnDestroy 调用 OnRoundEnd → carried→storeCount）
    systems.DestroyAll(registry);

    // 结算后保存（storeCount 已更新，Res_ItemInventory2 尚未擦除）
    ECS::SaveGame(registry);

    m_Pathfinder.reset();

    // 清除场景指针 ctx，防止 delete 后悬空指针
    if (registry.has_ctx<IScene*>()) {
        registry.ctx_erase<IScene*>();
    }

    // 清除场景级 ctx 资源，防止跨场景状态泄漏（registry.Clear() 不清除 ctx）
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

    LOG_INFO("[Scene_TutorialLevel] OnExit complete. All systems destroyed.");
}
