#include "Sys_ImGuiCapsuleGen.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include <cmath>
#include <algorithm>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/Res_CapsuleState.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Game/Utils/Log.h"

using namespace NCL::Maths;

namespace ECS {

void Sys_ImGuiCapsuleGen::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiCapsuleGen] OnAwake");
}

void Sys_ImGuiCapsuleGen::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_ImGuiCapsuleGen] OnDestroy");
}

void Sys_ImGuiCapsuleGen::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_CapsuleState>()) return;
    if (m_ShowCapsuleControls) RenderCapsuleControlWindow(registry);
}

// ============================================================
// RenderCapsuleControlWindow
// ============================================================

void Sys_ImGuiCapsuleGen::RenderCapsuleControlWindow(Registry& registry) {
    auto& state = registry.ctx<Res_CapsuleState>();

    ImGui::Begin("Capsule Controls", &m_ShowCapsuleControls);
    ImGui::Text("== Capsule Factory ==");
    ImGui::Separator();

    if (ImGui::Button("Spawn Capsule",       ImVec2(140, 30))) SpawnCapsule(registry);
    ImGui::SameLine();
    if (ImGui::Button("Delete Last Capsule", ImVec2(140, 30))) DeleteLastCapsule(registry);

    ImGui::Separator();
    ImGui::Text("Capsules alive: %d", (int)state.capsuleEntities.size());
    ImGui::End();
}

// ============================================================
// SpawnCapsule
// ============================================================

void Sys_ImGuiCapsuleGen::SpawnCapsule(Registry& registry) {
    auto& state = registry.ctx<Res_CapsuleState>();

    if (state.capsuleMeshHandle == ECS::INVALID_HANDLE) {
        LOG_WARN("[Sys_ImGuiCapsuleGen] SpawnCapsule: capsule mesh handle is INVALID, skipping.");
        return;
    }

    // 计算生成位置：相机正前方 5 单位
    Vector3 spawnPos(0.0f, 8.0f, 0.0f);
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
            spawnPos = tf.position + forward * 5.0f;
            spawnPos.y = tf.position.y + 2.0f;
            constexpr float MIN_SPAWN_Y = -2.0f;
            spawnPos.y = std::max(spawnPos.y, MIN_SPAWN_Y);
        }
    }

    EntityID id = PrefabFactory::CreatePhysicsCapsule(
        registry, state.capsuleMeshHandle, state.capsuleSpawnIndex, spawnPos);
    ++state.capsuleSpawnIndex;
    state.capsuleEntities.push_back(id);
    LOG_INFO("[Sys_ImGuiCapsuleGen] Spawned capsule id=" << id
             << " (total=" << state.capsuleEntities.size() << ")");
}

// ============================================================
// DeleteLastCapsule
// ============================================================

void Sys_ImGuiCapsuleGen::DeleteLastCapsule(Registry& registry) {
    auto& state = registry.ctx<Res_CapsuleState>();
    if (state.capsuleEntities.empty()) return;

    EntityID last = state.capsuleEntities.back();
    state.capsuleEntities.pop_back();

    if (registry.Valid(last)) {
        registry.Destroy(last);
        LOG_INFO("[Sys_ImGuiCapsuleGen] Destroyed capsule id=" << last);
    }
}

} // namespace ECS
#endif
