/**
 * @file Scene_NetworkGame.cpp
 * @brief 联机场景生命周期实现。
 */
#include "Scene_NetworkGame.h"

#include "Assets.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"     
#include "Core/Bridge/AssetManager.h"   
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Systems/Sys_Input.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Network.h"
#include "Game/Systems/Sys_Interpolation.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
#include "Game/Systems/Sys_ImGuiEntityDebug.h"
#include "Game/Systems/Sys_UI.h"
#include "Game/Systems/Sys_Chat.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/Components/Res_ChatState.h"
#include "Game/Components/Res_InventoryState.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Components/Res_DialogueData.h"
#include "Game/UI/UI_Toast.h"
#include "Game/Systems/Sys_ImGuiRenderDebug.h"
#endif

#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/C_D_RigidBody.h"

/**
 * @brief 进入联机场景并初始化网络、物理与 UI 系统。
 * @details 预热资源、创建网络上下文与初始同步实体，依据当前 PeerType 配置客户端插值表现，然后注册网络场景所需系统并唤醒它们。
 * @param registry 当前场景注册表
 * @param systems 当前场景系统管理器
 * @param nclPtrs NCL 桥接资源（当前函数未直接使用）
 */
void Scene_NetworkGame::OnEnter(ECS::Registry&          registry,
                                 ECS::SystemManager&     systems,
                                 const Res_NCL_Pointers& /*nclPtrs*/)
{
    // 1. 资源预热
    ECS::AssetManager::Instance().Init();
    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(NCL::Assets::MESHDIR + "cube.obj");

    // 2. 注册网络资源
    auto& resNet = registry.ctx_emplace<ECS::Res_Network>();
    resNet.mode = m_Mode;
    // 此处可扩展 IP/Port 存储

    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    if (!registry.has_ctx<Res_TestState>()) {
        Res_TestState state;
        state.cubeMeshHandle = cubeMesh;
        registry.ctx_emplace<Res_TestState>(std::move(state));
    }

    // 3. 初始实体
    PrefabFactory::CreateFloor(registry, cubeMesh);

    // 辅助 lambda：为客户端表现实体挂载插值缓冲 + 设为 Kinematic
    auto setupClientRepresentation = [&](ECS::EntityID entity) {
        registry.Emplace<ECS::C_D_InterpBuffer>(entity);
        if (registry.Has<C_D_RigidBody>(entity)) {
            registry.Get<C_D_RigidBody>(entity).is_kinematic = true;
        }
    };

    // Cube_1：Server 控制（ownerClientID = 0）
    ECS::EntityID cube1 = PrefabFactory::CreatePhysicsCube(
        registry, cubeMesh, 0, NCL::Maths::Vector3(-3.0f, 10.0f, 0.0f));
    registry.Emplace<ECS::C_D_NetworkIdentity>(cube1, 1u, 0u);   // netID=1, owner=0(Server)
    resNet.netIdMap[1] = cube1;

    ECS::EntityID cube2 = PrefabFactory::CreatePhysicsCube(
        registry, cubeMesh, 1, NCL::Maths::Vector3(3.0f, 10.0f, 0.0f));
    registry.Emplace<ECS::C_D_NetworkIdentity>(cube2, 2u, 1u);  // netID=2, owner=42(Client)
    resNet.netIdMap[2] = cube2;

    resNet.nextNetID = 3;

    // 权威架构物理配置：
    // Server：全场唯一权威，所有实体保持 Dynamic（受 Jolt 模拟）。
    // Client：纯粹的显示终端，所有实体改为 Kinematic（接受网络插值）。
    if (m_Mode == ECS::PeerType::CLIENT) {
        setupClientRepresentation(cube1);
        setupClientRepresentation(cube2);
    }

    // 4. 注册系统
    systems.Register<ECS::Sys_Input>        ( 10);
    systems.Register<ECS::Sys_Physics>      (100);
    systems.Register<ECS::Sys_Network>      (150);
    systems.Register<ECS::Sys_Interpolation>(160);
    systems.Register<ECS::Sys_Camera>       (180);
    systems.Register<ECS::Sys_Render>       (200);
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>           (300);
    systems.Register<ECS::Sys_ImGuiEntityDebug>(310);
    systems.Register<ECS::Sys_ImGuiRenderDebug>(445);   // 渲染参数调试面板
    systems.Register<ECS::Sys_Chat>            (450);
    systems.Register<ECS::Sys_UI>              (500);
#endif

    // 5. 初始化游戏状态（多人模式）
#ifdef USE_IMGUI
    {
        auto& gs = registry.ctx_emplace<ECS::Res_GameState>();
        gs.isMultiplayer = true;
    }
#endif

    // 6. 启动
    systems.AwakeAll(registry);

    // 7. 设置 UI 为 HUD 模式 + FadeIn
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_UIState>()) {
        auto& ui = registry.ctx<ECS::Res_UIState>();
        ui.previousScreen       = ui.activeScreen;
        ui.activeScreen         = ECS::UIScreen::HUD;
        ui.pendingSceneRequest  = ECS::SceneRequest::None;
        ui.transitionActive     = true;
        ui.transitionTimer      = 0.0f;
        ui.transitionDuration   = 0.5f;
        ui.transitionType       = 0;  // FadeIn
    }
    ECS::UI::PushToast(registry, "MULTIPLAYER CONNECTED", ECS::ToastType::Success, 2.5f);
#endif

    LOG_INFO("[Scene_NetworkGame] OnEnter complete. Mode=" << (m_Mode == ECS::PeerType::SERVER ? "SERVER" : "CLIENT"));
}

/**
 * @brief 退出联机场景并释放本场景持有的上下文资源。
 * @details 先销毁全部系统，再清理网络和 UI 相关 context，最后清空 Registry，避免跨场景残留同步状态或悬空指针。
 * @param registry 当前场景注册表
 * @param systems 当前场景系统管理器
 */
void Scene_NetworkGame::OnExit(ECS::Registry&      registry,
                                ECS::SystemManager& systems) 
{
    systems.DestroyAll(registry);
    // Explicitly clear context resources owned/used by this scene to avoid
    // leaving dangling raw pointers in the registry context after systems
    // have been destroyed.
    if (registry.has_ctx<ECS::Res_Network>()) {
        registry.ctx_erase<ECS::Res_Network>();
    }
    if (registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_erase<Res_UIFlags>();
    }
#ifdef USE_IMGUI
    if (registry.has_ctx<ECS::Res_GameState>())      registry.ctx_erase<ECS::Res_GameState>();
    if (registry.has_ctx<ECS::Res_ToastState>())     registry.ctx_erase<ECS::Res_ToastState>();
    if (registry.has_ctx<ECS::Res_ChatState>())      registry.ctx_erase<ECS::Res_ChatState>();
    if (registry.has_ctx<ECS::Res_InventoryState>()) registry.ctx_erase<ECS::Res_InventoryState>();
    if (registry.has_ctx<ECS::Res_LobbyState>())      registry.ctx_erase<ECS::Res_LobbyState>();
    if (registry.has_ctx<ECS::Res_DialogueData>())   registry.ctx_erase<ECS::Res_DialogueData>();
#endif
    registry.Clear();
    LOG_INFO("[Scene_NetworkGame] OnExit complete.");
}
