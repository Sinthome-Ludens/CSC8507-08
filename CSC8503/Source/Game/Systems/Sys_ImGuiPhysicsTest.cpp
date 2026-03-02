#include "Sys_ImGuiPhysicsTest.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/Res_EnemyTestState.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"

using namespace NCL::Maths;

namespace ECS {

void Sys_ImGuiPhysicsTest::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiPhysicsTest] OnAwake");
}

void Sys_ImGuiPhysicsTest::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiPhysicsTest] OnDestroy");
}

void Sys_ImGuiPhysicsTest::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_EnemyTestState>()) return;
    if (m_ShowWindow) RenderPhysicsTestWindow(registry);
}

// ============================================================
// RenderPhysicsTestWindow — PhysicsTest 场景敌人控制面板
// ============================================================

void Sys_ImGuiPhysicsTest::RenderPhysicsTestWindow(Registry& registry) {
    auto& state = registry.ctx<Res_EnemyTestState>();

    ImGui::Begin("Physics Test - Enemy Controls", &m_ShowWindow);
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
// SpawnEnemy — 计算生成位置（相机前方），调用 PrefabFactory
// ============================================================

void Sys_ImGuiPhysicsTest::SpawnEnemy(Registry& registry) {
    auto& state = registry.ctx<Res_EnemyTestState>();

    if (state.enemyMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGuiPhysicsTest] SpawnEnemy: enemy mesh handle is INVALID, skipping.");
        return;
    }

    // 计算生成位置：相机正前方 5 单位，或兜底位置
    Vector3 spawnPos(0.0f, 0.0f, 0.0f);
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
            // 将胶囊底部放在地板顶面上：地板顶约在 y=-5，胶囊 bottom offset ≈1.5 -> 中心 y≈-3.5
            spawnPos.y = -3.5f;
        }
    }

    EntityID id = PrefabFactory::CreatePhysicsEnemy(
        registry, state.enemyMeshHandle, state.enemySpawnIndex, spawnPos);
    ++state.enemySpawnIndex;
    state.enemyEntities.push_back(id);
    LOG_INFO("[Sys_ImGuiPhysicsTest] Spawned enemy id=" << id
             << " (total=" << state.enemyEntities.size() << ")");
}

// ============================================================
// DeleteLastEnemy — 标记延迟销毁最后一个生成的敌人
// ============================================================

void Sys_ImGuiPhysicsTest::DeleteLastEnemy(Registry& registry) {
    auto& state = registry.ctx<Res_EnemyTestState>();
    if (state.enemyEntities.empty()) return;

    EntityID last = state.enemyEntities.back();
    state.enemyEntities.pop_back();

    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGuiPhysicsTest] Destroyed enemy id=" << last);
    }
}

} // namespace ECS
#endif
