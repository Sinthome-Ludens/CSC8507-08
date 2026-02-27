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
#endif

#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/C_D_RigidBody.h"

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

    // Cube_2：Client 控制（ownerClientID = 42）
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
    systems.Register<ECS::Sys_ImGui>        (300);
#endif

    // 5. 启动
    systems.AwakeAll(registry);

    LOG_INFO("[Scene_NetworkGame] OnEnter complete. Mode=" << (m_Mode == ECS::PeerType::SERVER ? "SERVER" : "CLIENT"));
}

void Scene_NetworkGame::OnExit(ECS::Registry&      registry,
                               ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);
    registry.Clear();
    LOG_INFO("[Scene_NetworkGame] OnExit complete.");
}
