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
#include "Game/Systems/Sys_Physics.h"
#include "Game/Systems/Sys_Render.h"
#include "Game/Utils/Log.h"


#include <fstream>
#include <string>

#ifdef USE_IMGUI
#include "Game/Systems/Sys_ImGui.h"
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

    // 替换为加载 Level1.obj 作为场景主地形 [cite: 5]
    ECS::MeshHandle level1Mesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Level1.obj");
    LOG_INFO("[Scene_NavTest] Level1 mesh loaded, handle=" << level1Mesh);


    ECS::MeshHandle capsuleMesh = ECS::AssetManager::Instance().LoadMesh(
        NCL::Assets::MESHDIR + "Capsule.obj");
    LOG_INFO("[Scene_NavTest] capsule mesh loaded, handle=" << capsuleMesh);

    // 手动解析 Level1.navmesh!!!!!!!!!!!!
    NavMeshInternalData loadedNav;
    std::ifstream file(NCL::Assets::MESHDIR + "Level1.navmesh");
    if (file.is_open()) {
        std::string tag;
        int vCount = 0, iCount = 0;

        file >> tag >> vCount; // vertexCount 76
        file >> tag >> iCount; // indexCount 108
        file >> tag;           // vertices

        for(int i=0; i<vCount; ++i) {
            float x, y, z;
            file >> x >> y >> z;
            loadedNav.vertices.emplace_back(x, y, z);
        }

        file >> tag;           // indices
        for(int i=0; i<iCount; ++i) {
            int idx;
            file >> idx;
            loadedNav.indices.push_back(idx);
        }
        LOG_INFO("[Scene_NavTest] NavMesh loaded: " << vCount << " verts.");
        file.close();
    }

    // ── 2. 注册场景级全局资源到 Registry context ────────────────────────
    if (!registry.has_ctx<Res_UIFlags>()) {
        registry.ctx_emplace<Res_UIFlags>();
    }

    if (!registry.has_ctx<Res_NavTestState>()) {
        Res_NavTestState state;
        state.cubeMeshHandle    = level1Mesh; // 将 Level1 赋给原有的 handle 槽位
        state.capsuleMeshHandle = capsuleMesh;
        registry.ctx_emplace<Res_NavTestState>(std::move(state));
    }

    // ── 3. 初始实体生成：使用 Level1 资源创建静态地板/关卡 ─────────────────
    ECS::EntityID entity_floor_main = PrefabFactory::CreateLevel1(registry, level1Mesh);
    LOG_INFO("[Scene_NavTest] floor entity id=" << entity_floor_main);

    // ── 4. 注册系统（优先级升序 = 先执行）──────────────────────────────
    systems.Register<ECS::Sys_Camera>   ( 50);
    systems.Register<ECS::Sys_Physics>  (100);
    systems.Register<ECS::Sys_Render>   (200);
    systems.Register<ECS::Sys_EnemyAI>  (250);
#ifdef USE_IMGUI
    systems.Register<ECS::Sys_ImGui>    (300);
#endif

    // ── 5. 启动所有系统 ──────────────────────────────────────────────────
    systems.AwakeAll(registry);

    LOG_INFO("[Scene_PhysicsTest] OnEnter complete. "
             << systems.Count() << " systems awake.");
}

// ============================================================
// OnExit（场景卸载阶段）
// ============================================================

void Scene_NavTest::OnExit(ECS::Registry&      registry,
                               ECS::SystemManager& systems)
{
    systems.DestroyAll(registry);
    LOG_INFO("[Scene_PhysicsTest] OnExit complete. All systems destroyed.");
}