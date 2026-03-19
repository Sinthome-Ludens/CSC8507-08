/**
 * @file Sys_ImGuiNavTest.cpp
 * @brief ImGui 导航调试面板：敌人/目标生成、Spawn Y 偏移、NavAgent 状态显示。
 */
#include "Sys_ImGuiNavTest.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/Res_NavTestState.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"

using namespace NCL::Maths;

namespace ECS {

static constexpr float kDegToRad = 3.14159265f / 180.0f;

void Sys_ImGuiNavTest::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiNavTest] OnAwake");
}

void Sys_ImGuiNavTest::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiNavTest] OnDestroy");
}

void Sys_ImGuiNavTest::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_NavTestState>()) return;
    if (m_ShowWindow) RenderNavTestWindow(registry);
}

// ============================================================
// 相机前方生成位置（内部辅助，供 SpawnXxx 复用）
// yOffset：在相机 Y 基础上的额外偏移（正值向上，负值向下）
// ============================================================
static Vector3 CalcNavSpawnPos(Registry& registry, float yOffset = 0.0f) {
    Vector3 spawnPos(0.0f, 0.0f, 0.0f);
    if (registry.has_ctx<Res_CameraContext>()) {
        auto& camCtx = registry.ctx<Res_CameraContext>();
        if (registry.Valid(camCtx.active_camera)
            && registry.Has<C_D_Transform>(camCtx.active_camera)
            && registry.Has<C_D_Camera>  (camCtx.active_camera))
        {
            auto& tf  = registry.Get<C_D_Transform>(camCtx.active_camera);
            auto& cam = registry.Get<C_D_Camera>   (camCtx.active_camera);
            const float yawRad = cam.yaw * kDegToRad;
            const Vector3 forward(-sinf(yawRad), 0.0f, -cosf(yawRad));
            spawnPos   = tf.position + forward * 6.0f;
            spawnPos.y = tf.position.y + yOffset;  // 跟随相机高度，支持多层平台
        }
    }
    return spawnPos;
}

// ============================================================
// RenderNavTestWindow
// ============================================================
void Sys_ImGuiNavTest::RenderNavTestWindow(Registry& registry) {
    auto& state = registry.ctx<Res_NavTestState>();

    ImGui::Begin("Nav Test Controls", &m_ShowWindow);

    // ── 生成高度控制 ────────────────────────────────────────────────
    ImGui::Text("== Spawn Settings ==");
    ImGui::Separator();
    ImGui::SliderFloat("Y Offset", &m_SpawnYOffset, -10.0f, 10.0f);
    ImGui::SameLine();
    if (ImGui::Button("Reset##Y", ImVec2(50, 0))) m_SpawnYOffset = 0.0f;
    ImGui::TextDisabled("  (Camera Y + offset = spawn height)");

    // ── Enemy Factory ──────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Text("== Enemy Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Nav Enemy",       ImVec2(150, 30))) SpawnEnemy_Nav(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Enemy",     ImVec2(150, 30))) DeleteLastEnemy_Nav(registry);

    // ── Target Factory ─────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Text("== Target Factory ==");
    ImGui::Separator();
    if (ImGui::Button("Spawn Nav Target",      ImVec2(150, 30))) SpawnTarget(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Target",    ImVec2(150, 30))) DeleteLastTarget(registry);

    // ── 状态摘要 ────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Enemies alive: %d", (int)state.enemyEntities.size());
    ImGui::Text("Targets alive: %d", (int)state.targetEntities.size());

    // ── Detection 批量控制（便于测试敌人 Hunt 状态）────────────────
    ImGui::Spacing();
    ImGui::Text("== Detection Debug ==");
    ImGui::Separator();
    static float batchValue = 100.0f;
    ImGui::SliderFloat("Detection Value", &batchValue, 0.0f, 100.0f);
    if (ImGui::Button("Set All Enemies Detected", ImVec2(220, 30))) {
        auto view = registry.view<C_T_Enemy, C_D_AIPerception>();
        view.each([&](EntityID, C_T_Enemy&, C_D_AIPerception& det) {
            det.detection_value = batchValue;
            det.is_spotted      = (batchValue >= 50.0f);
        });
    }

    ImGui::End();
}

// ============================================================
// SpawnEnemy_Nav
// ============================================================
void Sys_ImGuiNavTest::SpawnEnemy_Nav(Registry& registry) {
    auto& state = registry.ctx<Res_NavTestState>();

    if (state.enemyMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGuiNavTest] SpawnEnemy_Nav: enemy mesh handle is INVALID, skipping.");
        return;
    }

    Vector3 spawnPos = CalcNavSpawnPos(registry, m_SpawnYOffset);
    EntityID id = PrefabFactory::CreateNavEnemy(
        registry, state.enemyMeshHandle, state.enemySpawnIndex, spawnPos);
    ++state.enemySpawnIndex;
    state.enemyEntities.push_back(id);
    LOG_INFO("[Sys_ImGuiNavTest] Spawned nav enemy id=" << id
             << " (total=" << state.enemyEntities.size() << ")");
}

// ============================================================
// DeleteLastEnemy_Nav
// ============================================================
void Sys_ImGuiNavTest::DeleteLastEnemy_Nav(Registry& registry) {
    auto& state = registry.ctx<Res_NavTestState>();
    if (state.enemyEntities.empty()) return;

    EntityID last = state.enemyEntities.back();
    state.enemyEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGuiNavTest] Destroyed nav enemy id=" << last);
    }
}

// ============================================================
// SpawnTarget
// ============================================================
void Sys_ImGuiNavTest::SpawnTarget(Registry& registry) {
    auto& state = registry.ctx<Res_NavTestState>();

    if (state.targetMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGuiNavTest] SpawnTarget: target mesh handle is INVALID, skipping.");
        return;
    }

    Vector3 spawnPos = CalcNavSpawnPos(registry, m_SpawnYOffset);
    EntityID id = PrefabFactory::CreateNavTarget(
        registry, state.targetMeshHandle, state.targetSpawnIndex, spawnPos);
    ++state.targetSpawnIndex;
    state.targetEntities.push_back(id);
    LOG_INFO("[Sys_ImGuiNavTest] Spawned nav target id=" << id
             << " (total=" << state.targetEntities.size() << ")");
}

// ============================================================
// DeleteLastTarget
// ============================================================
void Sys_ImGuiNavTest::DeleteLastTarget(Registry& registry) {
    auto& state = registry.ctx<Res_NavTestState>();
    if (state.targetEntities.empty()) return;

    EntityID last = state.targetEntities.back();
    state.targetEntities.pop_back();
    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGuiNavTest] Destroyed nav target id=" << last);
    }
}

} // namespace ECS
#endif
