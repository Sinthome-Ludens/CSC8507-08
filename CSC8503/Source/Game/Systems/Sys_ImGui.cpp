#include "Sys_ImGui.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_BaseTestState.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"
#include "GameWorld.h"
#include "Constraint.h"
#include "PhysicsSystem.h"
#include <iterator>
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPreception.h"
#include <cmath>

using namespace NCL::Maths;

namespace ECS {

// ============================================================
// OnAwake / OnDestroy
// ============================================================

void Sys_ImGui::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGui] OnAwake");
}

void Sys_ImGui::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGui] OnDestroy");
}

// ============================================================
// OnUpdate — 按层分发：通用基础层 / PhysicsTest 专属层 / NavTest 专属层
// ============================================================

void Sys_ImGui::OnUpdate(Registry& registry, float dt) {
    RenderMainMenuBar(registry);

    if (m_ShowDemoWindow)  ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    if (m_ShowDebugWindow) RenderDebugWindow(registry, dt);
    if (m_ShowNCLStatus)   RenderNCLStatus(registry);

    if (!registry.has_ctx<Res_UIFlags>()) return;
    auto& flags = registry.ctx<Res_UIFlags>();

    // 通用基础层（所有场景：Cube/Capsule/Gravity）
    if (flags.showBaseTestControls && registry.has_ctx<Res_BaseTestState>())
        RenderBaseTestWindow(registry);

    if (flags.showCubeDebug && registry.has_ctx<Res_BaseTestState>())
        RenderCubeDebugWindow(registry);

    // PhysicsTest 专属层（Enemy/Target）
    if (flags.showPhysicsTestControls && registry.has_ctx<Res_TestState>())
        RenderPhysicsTestWindow(registry);

    // NavTest 专属层（Enemy/Target）
    if (flags.showNavTestControls && registry.has_ctx<Res_NavTestState>())
        RenderNavTestWindow(registry);

    // 通用 AI 监控（组件级，场景无关）
    if (flags.showEnemyAIStatus)
        RenderEnemyAIStateWindow(registry);
}

// ============================================================
// RenderMainMenuBar
// ============================================================

void Sys_ImGui::RenderMainMenuBar(Registry& registry) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Windows")) {
        ImGui::MenuItem("Demo Window",  nullptr, &m_ShowDemoWindow);
        ImGui::MenuItem("Debug Window", nullptr, &m_ShowDebugWindow);
        ImGui::MenuItem("NCL Status",   nullptr, &m_ShowNCLStatus);
        ImGui::EndMenu();
    }

    if (registry.has_ctx<Res_UIFlags>()) {
        auto& flags = registry.ctx<Res_UIFlags>();

        if (registry.has_ctx<Res_BaseTestState>()) {
            if (ImGui::BeginMenu("Base Test")) {
                ImGui::MenuItem("Base Test Controls", nullptr, &flags.showBaseTestControls);
                ImGui::MenuItem("Cube Debug",         nullptr, &flags.showCubeDebug);
                ImGui::EndMenu();
            }
        }

        if (registry.has_ctx<Res_TestState>()) {
            if (ImGui::BeginMenu("Physics Test")) {
                ImGui::MenuItem("Enemy/Target Controls", nullptr, &flags.showPhysicsTestControls);
                ImGui::EndMenu();
            }
        }

        if (registry.has_ctx<Res_NavTestState>()) {
            if (ImGui::BeginMenu("Nav Test")) {
                ImGui::MenuItem("Nav Test Controls", nullptr, &flags.showNavTestControls);
                ImGui::EndMenu();
            }
        }

        if (ImGui::BeginMenu("AI")) {
            ImGui::MenuItem("Enemy Monitor", nullptr, &flags.showEnemyAIStatus);
            ImGui::EndMenu();
        }
    }

    ImGui::EndMainMenuBar();
}

// ============================================================
// RenderDebugWindow
// ============================================================

void Sys_ImGui::RenderDebugWindow(Registry& registry, float dt) {
    ImGui::Begin("Debug Window", &m_ShowDebugWindow);
    ImGui::Text("FPS: %.1f",           (dt > 0.0f) ? 1.0f / dt : 0.0f);
    ImGui::Text("Frame Time: %.3f ms", dt * 1000.0f);
    ImGui::Separator();
    ImGui::Text("ECS Entities: %d", (int)registry.EntityCount());
    ImGui::End();
}

// ============================================================
// RenderNCLStatus
// ============================================================

void Sys_ImGui::RenderNCLStatus(Registry& registry) {
    ImGui::Begin("NCL Status", &m_ShowNCLStatus);

    if (registry.has_ctx<Res_NCL_Pointers>()) {
        auto& nclPtrs = registry.ctx<Res_NCL_Pointers>();
        if (nclPtrs.world) {
            NCL::CSC8503::GameObjectIterator first, last;
            nclPtrs.world->GetObjectIterators(first, last);
            int objCount = (int)std::distance(first, last);

            std::vector<NCL::CSC8503::Constraint*>::const_iterator cFirst, cLast;
            nclPtrs.world->GetConstraintIterators(cFirst, cLast);
            int constraintCount = (int)std::distance(cFirst, cLast);

            ImGui::Text("GameWorld Objects:  %d", objCount);
            ImGui::Text("Constraints:        %d", constraintCount);
        }
        if (nclPtrs.physics) {
            ImGui::Text("Physics: Active");
        }
    } else {
        ImGui::TextDisabled("Res_NCL_Pointers not registered");
    }

    ImGui::End();
}

// ============================================================
// RenderBaseTestWindow — 通用基础控制面板（所有场景）
// ============================================================

void Sys_ImGui::RenderBaseTestWindow(Registry& registry) {
    auto& flags = registry.ctx<Res_UIFlags>();
    auto& state = registry.ctx<Res_BaseTestState>();

    ImGui::Begin("Base Test Controls", &flags.showBaseTestControls);

    ImGui::Text("== Cube Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Cube",       ImVec2(140, 30))) SpawnCube(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Cube", ImVec2(140, 30))) DeleteLastCube(registry);

    ImGui::Spacing();
    ImGui::Text("== Capsule Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Capsule",        ImVec2(140, 30))) SpawnCapsule(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Capsule",  ImVec2(140, 30))) DeleteLastCapsule(registry);

    ImGui::Spacing();
    ImGui::Text("== Gravity ==");
    ImGui::Separator();
    if (ImGui::Button("Enable Gravity",  ImVec2(140, 30))) SetGravityAll(registry, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Disable Gravity", ImVec2(140, 30))) SetGravityAll(registry, 0.0f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Cubes alive:    %d", (int)state.cubeEntities.size());
    ImGui::Text("Capsules alive: %d", (int)state.capsuleEntities.size());

    ImGui::End();
}

// ============================================================
// RenderPhysicsTestWindow — PhysicsTest 专属面板（Enemy/Target）
// ============================================================

void Sys_ImGui::RenderPhysicsTestWindow(Registry& registry) {
    auto& flags = registry.ctx<Res_UIFlags>();
    auto& state = registry.ctx<Res_TestState>();

    ImGui::Begin("Physics Test Controls", &flags.showPhysicsTestControls);

    ImGui::Text("== Enemy Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Enemy",       ImVec2(140, 30))) SpawnEnemy(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Enemy", ImVec2(140, 30))) DeleteLastEnemy(registry);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Enemies alive: %d", (int)state.enemyEntities.size());

    ImGui::End();
}

// ============================================================
// RenderNavTestWindow — NavTest 专属面板（Enemy/Target）
// ============================================================

void Sys_ImGui::RenderNavTestWindow(Registry& registry) {
    auto& flags = registry.ctx<Res_UIFlags>();
    auto& state = registry.ctx<Res_NavTestState>();

    ImGui::Begin("Nav Test Controls", &flags.showNavTestControls);

    ImGui::Text("== Enemy Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Enemy",       ImVec2(140, 30))) SpawnEnemy_Nav(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Enemy", ImVec2(140, 30))) DeleteLastEnemy_Nav(registry);

    ImGui::Spacing();
    ImGui::Text("== Target Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Target",       ImVec2(140, 30))) SpawnTarget(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Target", ImVec2(140, 30))) DeleteLastTarget(registry);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Enemies alive: %d", (int)state.enemyEntities.size());
    ImGui::Text("Targets alive: %d", (int)state.targetEntities.size());

    // Detection Value 批量控制
    ImGui::Spacing();
    ImGui::Text("== Detection ==");
    ImGui::Separator();
    static float batchValue = 10.0f;
    ImGui::SliderFloat("Value Step", &batchValue, 0.0f, 100.0f);
    if (ImGui::Button("Add Detection to All", ImVec2(200, 30))) {
        auto view = registry.view<C_T_Enemy, C_D_AIPreception>();
        view.each([&](EntityID, C_T_Enemy&, C_D_AIPreception& det) {
            det.detectionValue += batchValue;
        });
    }

    ImGui::End();
}

// ============================================================
// RenderCubeDebugWindow — per-cube 详细状态（依赖 Res_BaseTestState）
// ============================================================

void Sys_ImGui::RenderCubeDebugWindow(Registry& registry) {
    if (!registry.has_ctx<Res_BaseTestState>()) return;
    auto& state = registry.ctx<Res_BaseTestState>();
    auto& flags = registry.ctx<Res_UIFlags>();

    ImGui::Begin("Cube Debug Info", &flags.showCubeDebug,
                 ImGuiWindowFlags_NoCollapse);

    ImGui::Text("%-6s  %-20s  %-8s  %-7s",
                "ID", "Position", "Gravity", "Body");
    ImGui::Separator();

    // 清除已失效的 cube
    state.cubeEntities.erase(
        std::remove_if(state.cubeEntities.begin(), state.cubeEntities.end(),
            [&](ECS::EntityID id) { return !registry.Valid(id); }),
        state.cubeEntities.end()
    );

    for (ECS::EntityID id : state.cubeEntities) {
        if (!registry.Valid(id)) continue;

        Vector3     pos        = {};
        float       grav       = 0.0f;
        const char* bodyStatus = "pending";

        if (registry.Has<C_D_Transform>(id)) {
            pos = registry.Get<C_D_Transform>(id).position;
        }
        if (registry.Has<C_D_RigidBody>(id)) {
            auto& rb  = registry.Get<C_D_RigidBody>(id);
            grav       = rb.gravity_factor;
            bodyStatus = rb.body_created ? "created" : "pending";
        }

        ImGui::Text("%-6u  (%-5.1f, %-5.1f, %-5.1f)  %-8.2f  %-7s",
                    id, pos.x, pos.y, pos.z, grav, bodyStatus);
    }

    if (state.cubeEntities.empty()) {
        ImGui::TextDisabled("No cube entities.");
    }

    ImGui::End();
}

// ============================================================
// RenderEnemyAIStateWindow — 通用敌人状态监控（组件级，场景无关）
// ============================================================

void Sys_ImGui::RenderEnemyAIStateWindow(Registry& registry) {
    if (!ImGui::Begin("Enemy Monitoring Station")) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("EnemyTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Entity ID");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("AI State");
        ImGui::TableSetupColumn("Detection");
        ImGui::TableHeadersRow();

        auto view = registry.view<C_T_Enemy, C_D_Transform, C_D_AIState, C_D_AIPreception>();
        view.each([&](EntityID id, auto&, C_D_Transform& tf, C_D_AIState& state, C_D_AIPreception& det) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", (int)id);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.1f, %.1f, %.1f", tf.position.x, tf.position.y, tf.position.z);

            ImGui::TableSetColumnIndex(2);
            const char* stateStr = (state.currentState == EnemyState::Safe)    ? "SAFE"    :
                                   (state.currentState == EnemyState::Caution) ? "CAUTION" :
                                   (state.currentState == EnemyState::Alert)   ? "ALERT"   : "HUNT";
            ImGui::TextUnformatted(stateStr);

            ImGui::TableSetColumnIndex(3);
            ImGui::ProgressBar(det.detectionValue / 100.0f, ImVec2(-1.0f, 0.0f));
        });

        ImGui::EndTable();
    }

    ImGui::End();
}

// ============================================================
// 相机前方生成位置（内部辅助，供所有 SpawnXxx 复用）
// ============================================================

static Vector3 CalcSpawnPos(Registry& registry) {
    Vector3 spawnPos(0.0f, 8.0f, 0.0f);  // 默认兜底

    if (registry.has_ctx<Res_CameraContext>()) {
        auto& camCtx = registry.ctx<Res_CameraContext>();
        if (registry.Valid(camCtx.active_camera)
            && registry.Has<C_D_Transform>(camCtx.active_camera)
            && registry.Has<C_D_Camera>  (camCtx.active_camera))
        {
            auto& tf  = registry.Get<C_D_Transform>(camCtx.active_camera);
            auto& cam = registry.Get<C_D_Camera>   (camCtx.active_camera);

            const float yawRad = cam.yaw * (3.14159265f / 180.0f);
            const Vector3 forward(-sinf(yawRad), 0.0f, -cosf(yawRad));

            spawnPos   = tf.position + forward * 5.0f;
            spawnPos.y = tf.position.y + 2.0f;

            constexpr float MIN_SPAWN_Y = -2.0f;
            spawnPos.y = std::max(spawnPos.y, MIN_SPAWN_Y);
        }
    }
    return spawnPos;
}

// ============================================================
// SpawnCube / DeleteLastCube（Res_BaseTestState）
// ============================================================

void Sys_ImGui::SpawnCube(Registry& registry) {
    if (!registry.has_ctx<Res_BaseTestState>()) return;
    auto& state = registry.ctx<Res_BaseTestState>();

    if (state.cubeMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnCube: cube mesh handle is INVALID, skipping.");
        return;
    }

    EntityID id = PrefabFactory::CreatePhysicsCube(
        registry, state.cubeMeshHandle, state.spawnIndex, CalcSpawnPos(registry));
    ++state.spawnIndex;
    state.cubeEntities.push_back(id);
    LOG_INFO("[Sys_ImGui] Spawned cube id=" << id
             << " (total=" << state.cubeEntities.size() << ")");
}

void Sys_ImGui::DeleteLastCube(Registry& registry) {
    if (!registry.has_ctx<Res_BaseTestState>()) return;
    auto& state = registry.ctx<Res_BaseTestState>();
    if (state.cubeEntities.empty()) return;

    EntityID last = state.cubeEntities.back();
    state.cubeEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] Destroyed cube id=" << last);
    }
}

// ============================================================
// SpawnCapsule / DeleteLastCapsule（Res_BaseTestState）
// ============================================================

void Sys_ImGui::SpawnCapsule(Registry& registry) {
    if (!registry.has_ctx<Res_BaseTestState>()) return;
    auto& state = registry.ctx<Res_BaseTestState>();

    if (state.capsuleMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnCapsule: capsule mesh handle is INVALID, skipping.");
        return;
    }

    EntityID id = PrefabFactory::CreatePhysicsCapsule(
        registry, state.capsuleMeshHandle, state.capsuleSpawnIndex, CalcSpawnPos(registry));
    ++state.capsuleSpawnIndex;
    state.capsuleEntities.push_back(id);
    LOG_INFO("[Sys_ImGui] Spawned capsule id=" << id
             << " (total=" << state.capsuleEntities.size() << ")");
}

void Sys_ImGui::DeleteLastCapsule(Registry& registry) {
    if (!registry.has_ctx<Res_BaseTestState>()) return;
    auto& state = registry.ctx<Res_BaseTestState>();
    if (state.capsuleEntities.empty()) return;

    EntityID last = state.capsuleEntities.back();
    state.capsuleEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] Destroyed capsule id=" << last);
    }
}

// ============================================================
// SetGravityAll（Res_BaseTestState）
// ============================================================

void Sys_ImGui::SetGravityAll(Registry& registry, float factor) {
    if (!registry.has_ctx<Res_BaseTestState>()) return;
    auto& state = registry.ctx<Res_BaseTestState>();

    for (EntityID id : state.cubeEntities) {
        if (registry.Valid(id) && registry.Has<C_D_RigidBody>(id)) {
            registry.Get<C_D_RigidBody>(id).gravity_factor = factor;
        }
    }
    LOG_INFO("[Sys_ImGui] gravity_factor=" << factor
             << " applied to " << state.cubeEntities.size() << " cubes.");
}

// ============================================================
// SpawnEnemy / DeleteLastEnemy（PhysicsTest — Res_TestState）
// ============================================================

void Sys_ImGui::SpawnEnemy(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.enemyMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnEnemy: enemy mesh handle is INVALID, skipping.");
        return;
    }

    EntityID id = PrefabFactory::CreatePhysicsEnemy(
        registry, state.enemyMeshHandle, state.enemySpawnIndex, CalcSpawnPos(registry));
    ++state.enemySpawnIndex;
    state.enemyEntities.push_back(id);
    LOG_INFO("[Sys_ImGui] Spawned enemy id=" << id
             << " (total=" << state.enemyEntities.size() << ")");
}

void Sys_ImGui::DeleteLastEnemy(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();
    if (state.enemyEntities.empty()) return;

    EntityID last = state.enemyEntities.back();
    state.enemyEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] Destroyed enemy id=" << last);
    }
}

// ============================================================
// SpawnEnemy_Nav / DeleteLastEnemy_Nav（NavTest — Res_NavTestState）
// ============================================================

void Sys_ImGui::SpawnEnemy_Nav(Registry& registry) {
    if (!registry.has_ctx<Res_NavTestState>()) return;
    auto& state = registry.ctx<Res_NavTestState>();

    if (state.enemyMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnEnemy_Nav: enemy mesh handle is INVALID, skipping.");
        return;
    }

    EntityID id = PrefabFactory::CreatePhysicsEnemy(
        registry, state.enemyMeshHandle, state.enemySpawnIndex, CalcSpawnPos(registry));
    ++state.enemySpawnIndex;
    state.enemyEntities.push_back(id);
    LOG_INFO("[Sys_ImGui] [NavTest] Spawned enemy id=" << id
             << " (total=" << state.enemyEntities.size() << ")");
}

void Sys_ImGui::DeleteLastEnemy_Nav(Registry& registry) {
    if (!registry.has_ctx<Res_NavTestState>()) return;
    auto& state = registry.ctx<Res_NavTestState>();
    if (state.enemyEntities.empty()) return;

    EntityID last = state.enemyEntities.back();
    state.enemyEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] [NavTest] Destroyed enemy id=" << last);
    }
}

// ============================================================
// SpawnTarget / DeleteLastTarget（NavTest — Res_NavTestState）
// ============================================================

void Sys_ImGui::SpawnTarget(Registry& registry) {
    if (!registry.has_ctx<Res_NavTestState>()) return;
    auto& state = registry.ctx<Res_NavTestState>();

    if (state.targetMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnTarget: target mesh handle is INVALID, skipping.");
        return;
    }

    EntityID id = PrefabFactory::CreatePhysicsTarget(
        registry, state.targetMeshHandle, state.targetSpawnIndex, CalcSpawnPos(registry));
    ++state.targetSpawnIndex;
    state.targetEntities.push_back(id);
    LOG_INFO("[Sys_ImGui] [NavTest] Spawned target id=" << id
             << " (total=" << state.targetEntities.size() << ")");
}

void Sys_ImGui::DeleteLastTarget(Registry& registry) {
    if (!registry.has_ctx<Res_NavTestState>()) return;
    auto& state = registry.ctx<Res_NavTestState>();
    if (state.targetEntities.empty()) return;

    EntityID last = state.targetEntities.back();
    state.targetEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] [NavTest] Destroyed target id=" << last);
    }
}

} // namespace ECS
#endif
