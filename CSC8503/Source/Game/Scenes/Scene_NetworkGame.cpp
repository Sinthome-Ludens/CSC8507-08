#include "Scene_NetworkGame.h"

#include "Assets.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"     
#include "Core/Bridge/AssetManager.h"   
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Prefabs/PrefabFactory.h"
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

    // 建立一个由 Server 控制的同步方块（模拟移动：它受重力下落）
    ECS::EntityID cube = PrefabFactory::CreatePhysicsCube(registry, cubeMesh, 0, NCL::Maths::Vector3(0, 10, 0));
    registry.Emplace<ECS::C_D_NetworkIdentity>(cube, 1u, 0u); // netID=1, owner=0(Server)
    resNet.netIdMap[1] = cube;

    if (m_Mode == ECS::PeerType::CLIENT) {
        registry.Emplace<ECS::C_D_InterpBuffer>(cube);
        // 让客户端使用 Kinematic 避免本地物理和网络插值打架
        if (registry.Has<C_D_RigidBody>(cube)) {
            registry.Get<C_D_RigidBody>(cube).is_kinematic = true;
        }
    }

    // 4. 注册系统
    //TODO: 系统优先级调整，确认相机的优先级，是否应该在物理之后渲染之前更新相机？
    systems.Register<ECS::Sys_Camera>       ( 50);
    systems.Register<ECS::Sys_Physics>      (100);
    systems.Register<ECS::Sys_Network>      (150);
    systems.Register<ECS::Sys_Interpolation>(160);
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
