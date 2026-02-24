#include "Sys_ImGui.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_TestState.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"
#include "GameWorld.h"
#include "Constraint.h"
#include "PhysicsSystem.h"
#include "GameTechRendererInterface.h"
#include <iterator>
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
// OnUpdate
// ============================================================

void Sys_ImGui::OnUpdate(Registry& registry, float dt) {
    RenderMainMenuBar(registry);

    if (m_ShowDemoWindow)  ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    if (m_ShowDebugWindow) RenderDebugWindow(registry, dt);
    if (m_ShowNCLStatus)   RenderNCLStatus(registry);

    // Test Scene 窗口：由 Res_UIFlags context 控制显隐
    if (registry.has_ctx<Res_UIFlags>()) {
        auto& flags = registry.ctx<Res_UIFlags>();
        if (flags.showTestControls) RenderTestControlsWindow(registry);
        if (flags.showCubeDebug)    RenderCubeDebugWindow(registry);
    }
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

    // Test Scene 子菜单：通过 Res_UIFlags context 控制各浮窗可见性
    if (registry.has_ctx<Res_UIFlags>()) {
        auto& flags = registry.ctx<Res_UIFlags>();
        if (ImGui::BeginMenu("Test Scene")) {
            ImGui::MenuItem("Test Controls", nullptr, &flags.showTestControls);
            ImGui::MenuItem("Cube Debug",    nullptr, &flags.showCubeDebug);
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
    ImGui::Separator();
    if (registry.has_ctx<Res_NCL_Pointers>()) {
        auto& nclPtrs = registry.ctx<Res_NCL_Pointers>();
        if (nclPtrs.renderer) {
            if (ImGui::Checkbox("Wireframe Mode", &m_WireframeMode)) {
                nclPtrs.renderer->SetWireframeMode(m_WireframeMode);
            }
        }
    }
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
// RenderTestControlsWindow（集成控制面板）
// ============================================================

void Sys_ImGui::RenderTestControlsWindow(Registry& registry) {
    auto& flags = registry.ctx<Res_UIFlags>();

    ImGui::Begin("ECS Test Controls", &flags.showTestControls);

    ImGui::Text("== Cube Factory ==");
    ImGui::Separator();

    if (ImGui::Button("Spawn Cube",  ImVec2(120, 30))) SpawnCube(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last", ImVec2(120, 30))) DeleteLastCube(registry);

    ImGui::Spacing();
    ImGui::Text("== Gravity ==");
    ImGui::Separator();

    if (ImGui::Button("Enable Gravity",  ImVec2(120, 30))) SetGravityAll(registry, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Disable Gravity", ImVec2(120, 30))) SetGravityAll(registry, 0.0f);

    ImGui::Spacing();
    ImGui::Separator();

    if (registry.has_ctx<Res_TestState>()) {
        auto& state = registry.ctx<Res_TestState>();
        ImGui::Text("Cubes alive: %d", (int)state.cubeEntities.size());
    }

    ImGui::End();
}

// ============================================================
// RenderCubeDebugWindow（独立浮动 Debug 窗口）
// ============================================================

void Sys_ImGui::RenderCubeDebugWindow(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();
    auto& flags = registry.ctx<Res_UIFlags>();

    ImGui::Begin("Cube Debug Info", &flags.showCubeDebug,
                 ImGuiWindowFlags_NoCollapse);

    ImGui::Text("%-6s  %-20s  %-8s  %-7s",
                "ID", "Position", "Gravity", "Body");
    ImGui::Separator();

    // 清除已失效的 cube（可能被外部途径销毁）
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
// SpawnCube（通过 PrefabFactory 生成动态方块）
// ============================================================

void Sys_ImGui::SpawnCube(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.cubeMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGui] SpawnCube: cube mesh handle is INVALID, skipping.");
        return;
    }

    // ── 计算生成位置：相机正前方 5 单位 ────────────────────────────────
    using namespace NCL::Maths;
    Vector3 spawnPos(0.0f, 8.0f, 0.0f);  // 默认兜底位置

    if (registry.has_ctx<Res_CameraContext>()) {
        auto& camCtx = registry.ctx<Res_CameraContext>();
        if (registry.Valid(camCtx.active_camera)
            && registry.Has<C_D_Transform>(camCtx.active_camera)
            && registry.Has<C_D_Camera>  (camCtx.active_camera))
        {
            auto& tf  = registry.Get<C_D_Transform>(camCtx.active_camera);
            auto& cam = registry.Get<C_D_Camera>   (camCtx.active_camera);

            // 计算水平前方向量（忽略 pitch，避免生成在地下或天上）
            const float yawRad = cam.yaw * (3.14159265f / 180.0f);
            const Vector3 forward(-sinf(yawRad), 0.0f, -cosf(yawRad));

            // 在相机前方 5 单位处、相机高度上方 2 单位生成
            spawnPos = tf.position + forward * 5.0f;
            spawnPos.y = tf.position.y + 2.0f;

            // 确保 cube 生成在地板上方（floor 上表面 y ≈ -5.5，保底 -2.0）
            constexpr float MIN_SPAWN_Y = -2.0f;
            spawnPos.y = std::max(spawnPos.y, MIN_SPAWN_Y);
        }
    }

    EntityID entity_cube = PrefabFactory::CreatePhysicsCube(
        registry, state.cubeMeshHandle, state.spawnIndex, spawnPos);
    ++state.spawnIndex;

    state.cubeEntities.push_back(entity_cube);
    LOG_INFO("[Sys_ImGui] Spawned cube entity id=" << entity_cube
             << " (total=" << state.cubeEntities.size() << ")");
}

// ============================================================
// DeleteLastCube
// ============================================================

void Sys_ImGui::DeleteLastCube(Registry& registry) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    if (state.cubeEntities.empty()) return;

    EntityID last = state.cubeEntities.back();
    state.cubeEntities.pop_back();

    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGui] Destroyed cube entity id=" << last);
    }
}

// ============================================================
// SetGravityAll
// ============================================================

void Sys_ImGui::SetGravityAll(Registry& registry, float factor) {
    if (!registry.has_ctx<Res_TestState>()) return;
    auto& state = registry.ctx<Res_TestState>();

    for (EntityID id : state.cubeEntities) {
        if (registry.Valid(id) && registry.Has<C_D_RigidBody>(id)) {
            registry.Get<C_D_RigidBody>(id).gravity_factor = factor;
        }
    }

    LOG_INFO("[Sys_ImGui] gravity_factor=" << factor
             << " applied to " << state.cubeEntities.size() << " cubes.");
}

} // namespace ECS
#endif
