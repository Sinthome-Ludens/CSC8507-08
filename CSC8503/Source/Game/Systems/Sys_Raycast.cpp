#include "Sys_Raycast.h"

#include "Debug.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/Res_CameraContext.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace ECS {

static constexpr float kPI = 3.14159265f;
static constexpr float kDegToRad = kPI / 180.0f;

static bool BuildCameraRay(Registry& registry,
                           float& ox, float& oy, float& oz,
                           float& dx, float& dy, float& dz) {
    if (!registry.has_ctx<Res_CameraContext>()) return false;

    const auto& camCtx = registry.ctx<Res_CameraContext>();
    if (!registry.Valid(camCtx.active_camera)) return false;
    if (!registry.Has<C_D_Transform>(camCtx.active_camera)) return false;
    if (!registry.Has<C_D_Camera>(camCtx.active_camera)) return false;

    const auto& tf = registry.Get<C_D_Transform>(camCtx.active_camera);
    const auto& cam = registry.Get<C_D_Camera>(camCtx.active_camera);

    const float yawRad = cam.yaw * (kDegToRad);
    const float pitchRad = cam.pitch * (kDegToRad);

    dx = -sinf(yawRad) * cosf(pitchRad);
    dy = sinf(pitchRad);
    dz = -cosf(yawRad) * cosf(pitchRad);

    const float lenSq = dx * dx + dy * dy + dz * dz;
    if (lenSq < 1e-8f) return false;
    const float invLen = 1.0f / sqrtf(lenSq);
    dx *= invLen;
    dy *= invLen;
    dz *= invLen;

    ox = tf.position.x;
    oy = tf.position.y;
    oz = tf.position.z;
    return true;
}

void Sys_Raycast::OnAwake(Registry& /*registry*/) {
    LOG_INFO("[Sys_Raycast] OnAwake");
}

/**
 * @brief 每帧更新射线检测调试面板及射线投射。
 *
 * 非 Shipping 构建下渲染 ImGui 调试窗口（参数调整 + 结果显示）；
 * Shipping 构建中该窗口被编译剔除。
 */
void Sys_Raycast::OnUpdate(Registry& registry, float /*dt*/) {
    if (registry.has_ctx<Res_UIFlags>()) {
        m_ShowWindow = registry.ctx<Res_UIFlags>().showRaycast;
    }

    bool castThisFrame = false;

#if defined(USE_IMGUI) && !defined(GAME_SHIPPING)
    if (m_ShowWindow) {
        ImGui::Begin("Raycast", &m_ShowWindow);
        ImGui::Checkbox("Enable Raycast", &m_EnableRaycast);
        ImGui::Checkbox("Show Ray", &m_ShowRay);
        ImGui::SliderFloat("Max Distance", &m_LastResult.maxDistance, 1.0f, 200.0f, "%.1f");
        castThisFrame = ImGui::Button("Cast Once", ImVec2(140, 28));

        ImGui::Separator();
        ImGui::Text("Last Result: %s", m_LastResult.hit ? "HIT" : "MISS");
        ImGui::Text("Distance: %.2f", m_LastResult.distance);
        ImGui::Text("EntityID: %u", m_LastResult.entityID);
        ImGui::End();
    }
#endif

    if (registry.has_ctx<Res_UIFlags>()) {
        registry.ctx<Res_UIFlags>().showRaycast = m_ShowWindow;
    }

    if (castThisFrame) {
        if (!m_EnableRaycast) {
            LOG_INFO("[Sys_Raycast] Cast skipped: raycast disabled");
        } else if (!registry.has_ctx<Sys_Physics*>()) {
            LOG_WARN("[Sys_Raycast] Cast skipped: Sys_Physics* missing in context");
        } else {
            auto* physics = registry.ctx<Sys_Physics*>();
            if (!physics) {
                LOG_WARN("[Sys_Raycast] Cast skipped: Sys_Physics pointer is null");
            } else {
                float ox = 0.0f, oy = 0.0f, oz = 0.0f;
                float dx = 0.0f, dy = 0.0f, dz = -1.0f;

                if (!BuildCameraRay(registry, ox, oy, oz, dx, dy, dz)) {
                    LOG_WARN("[Sys_Raycast] Cast skipped: no active camera ray");
                } else {
                    const float maxDist = std::max(1.0f, m_LastResult.maxDistance);
                    auto hit = physics->CastRay(ox, oy, oz, dx, dy, dz, maxDist);

                    m_LastResult.originX = ox;
                    m_LastResult.originY = oy;
                    m_LastResult.originZ = oz;
                    m_LastResult.hit = hit.hit;
                    m_LastResult.entityID = hit.hit ? hit.entity : 0xFFFFFFFFu;
                    m_LastResult.distance = hit.hit ? (hit.fraction * maxDist) : maxDist;

                    if (hit.hit) {
                        m_LastResult.hitX = hit.pointX;
                        m_LastResult.hitY = hit.pointY;
                        m_LastResult.hitZ = hit.pointZ;
                        m_LastResult.endX = hit.pointX;
                        m_LastResult.endY = hit.pointY;
                        m_LastResult.endZ = hit.pointZ;
                    } else {
                        m_LastResult.endX = ox + dx * maxDist;
                        m_LastResult.endY = oy + dy * maxDist;
                        m_LastResult.endZ = oz + dz * maxDist;
                    }

                    LOG_INFO("[Sys_Raycast] CastOnce result=" << (m_LastResult.hit ? "HIT" : "MISS")
                             << " dist=" << m_LastResult.distance
                             << " entity=" << m_LastResult.entityID);
                }
            }
        }
    }

    if (m_ShowRay && m_LastResult.distance > 0.001f) {
        NCL::Maths::Vector3 start(m_LastResult.originX, m_LastResult.originY, m_LastResult.originZ);
        NCL::Maths::Vector3 end(m_LastResult.endX, m_LastResult.endY, m_LastResult.endZ);
        NCL::Debug::DrawLine(start, end, m_LastResult.hit ? NCL::Debug::GREEN : NCL::Debug::RED, 0.0f);
    }
}

void Sys_Raycast::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_Raycast] OnDestroy");
}

} // namespace ECS
