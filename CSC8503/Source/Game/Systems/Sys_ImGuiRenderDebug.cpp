/**
 * @file Sys_ImGuiRenderDebug.cpp
 * @brief 渲染调试面板系统实现：通过 ImGui 滑块/复选框实时调节 GameTechRenderer 参数。
 *
 * @details
 * OnAwake 从 Res_NCL_Pointers 取得渲染器指针并 dynamic_cast 为 GameTechRenderer*，
 * 缓存于 m_renderer（void*）。OnUpdate 每帧调用各 Draw*Section 方法渲染面板，
 * 通过 setter 将调整后的参数写入渲染器。
 */
#include "Sys_ImGuiRenderDebug.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Utils/Log.h"
#include "GameTechRenderer.h"

using namespace NCL::CSC8503;

namespace ECS {

// ============================================================
// OnAwake
// ============================================================
void Sys_ImGuiRenderDebug::OnAwake(Registry& registry) {
    GAME_ASSERT(registry.has_ctx<Res_NCL_Pointers>(),
                "[Sys_ImGuiRenderDebug] Res_NCL_Pointers not found in registry context.");

    auto* iface = registry.ctx<Res_NCL_Pointers>().renderer;
    m_renderer  = dynamic_cast<GameTechRenderer*>(iface);

    if (!m_renderer) {
        LOG_WARN("[Sys_ImGuiRenderDebug] Renderer is not a GameTechRenderer — debug panel will be inactive.");
    } else {
        LOG_INFO("[Sys_ImGuiRenderDebug] OnAwake — renderer debug panel ready.");
    }
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_ImGuiRenderDebug::OnUpdate(Registry& /*registry*/, float /*dt*/) {
    if (!m_renderer) return;
    if (!m_panelOpen) return;

    ImGui::SetNextWindowSize(ImVec2(380, 580), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Render Debug", &m_panelOpen)) { ImGui::End(); return; }

    DrawGeometrySection(m_renderer);
    ImGui::Separator();
    DrawShadowSection(m_renderer);
    ImGui::Separator();
    DrawIBLSection(m_renderer);
    ImGui::Separator();
    DrawSSAOSection(m_renderer);
    ImGui::Separator();
    DrawPostProcessSection(m_renderer);

    ImGui::End();
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_ImGuiRenderDebug::OnDestroy(Registry& /*registry*/) {
    m_renderer = nullptr;
}

// ============================================================
// DrawGeometrySection
// ============================================================
void Sys_ImGuiRenderDebug::DrawGeometrySection(void* rendererPtr) {
    auto* r = static_cast<GameTechRenderer*>(rendererPtr);
    if (!ImGui::CollapsingHeader("Geometry", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (ImGui::Checkbox("Wireframe", &m_wireframe)) {
        r->SetWireframeMode(m_wireframe);
    }
}

// ============================================================
// DrawShadowSection
// ============================================================
void Sys_ImGuiRenderDebug::DrawShadowSection(void* rendererPtr) {
    auto* r = static_cast<GameTechRenderer*>(rendererPtr);
    if (!ImGui::CollapsingHeader("Shadows (CSM + PCSS)", ImGuiTreeNodeFlags_DefaultOpen)) return;

    bool splitChanged = false;
    splitChanged |= ImGui::SliderFloat("Near Split",   &m_cascadeSplits[0],  5.0f,  150.0f);
    splitChanged |= ImGui::SliderFloat("Mid Split",    &m_cascadeSplits[1], 20.0f,  400.0f);
    splitChanged |= ImGui::SliderFloat("Far Split",    &m_cascadeSplits[2], 80.0f, 1000.0f);
    if (splitChanged) {
        r->SetCascadeSplits(m_cascadeSplits[0], m_cascadeSplits[1], m_cascadeSplits[2]);
    }

    if (ImGui::SliderFloat("PCSS Light Size", &m_pcssLightSize, 0.5f, 20.0f)) {
        r->SetPCSSLightSize(m_pcssLightSize);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Shadow Bias");
    if (ImGui::SliderFloat("Bias Slope",    &m_shadowBiasSlope,    0.0f, 0.0002f, "%.6f")) {
        r->SetShadowBiasSlope(m_shadowBiasSlope);
    }
    if (ImGui::SliderFloat("Bias Constant", &m_shadowBiasConstant, 0.0f, 0.0002f, "%.6f")) {
        r->SetShadowBiasConstant(m_shadowBiasConstant);
    }

    if (ImGui::Checkbox("Debug Cascades", &m_debugCascades)) {
        r->SetDebugCascades(m_debugCascades);
    }
}

// ============================================================
// DrawIBLSection
// ============================================================
void Sys_ImGuiRenderDebug::DrawIBLSection(void* rendererPtr) {
    auto* r = static_cast<GameTechRenderer*>(rendererPtr);
    if (!ImGui::CollapsingHeader("IBL (Environment Light)")) return;

    if (ImGui::SliderFloat("IBL Intensity", &m_iblIntensity, 0.0f, 5.0f)) {
        r->SetIBLIntensity(m_iblIntensity);
    }
}

// ============================================================
// DrawSSAOSection
// ============================================================
void Sys_ImGuiRenderDebug::DrawSSAOSection(void* rendererPtr) {
    auto* r = static_cast<GameTechRenderer*>(rendererPtr);
    if (!ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (ImGui::Checkbox("SSAO Enabled", &m_ssaoEnabled)) {
        r->SetSSAOEnabled(m_ssaoEnabled);
    }

    if (ImGui::SliderFloat("SSAO Radius", &m_ssaoRadius, 0.05f, 2.0f)) {
        r->SetSSAORadius(m_ssaoRadius);
    }
    if (ImGui::SliderFloat("SSAO Bias",   &m_ssaoBias,   0.0f,  0.1f)) {
        r->SetSSAOBias(m_ssaoBias);
    }
    if (ImGui::Checkbox("Show SSAO Buffer", &m_showSSAOBuffer)) {
        r->SetShowSSAOBuffer(m_showSSAOBuffer);
    }
}

// ============================================================
// DrawPostProcessSection
// ============================================================
void Sys_ImGuiRenderDebug::DrawPostProcessSection(void* rendererPtr) {
    auto* r = static_cast<GameTechRenderer*>(rendererPtr);
    if (!ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) return;

    if (ImGui::SliderFloat("Exposure",         &m_exposure,        0.1f, 5.0f)) {
        r->SetExposure(m_exposure);
    }
    if (ImGui::SliderFloat("Vignette Strength",&m_vignetteStrength, 0.0f, 2.0f)) {
        r->SetVignetteStrength(m_vignetteStrength);
    }

    ImGui::Spacing();
    if (ImGui::Checkbox("Bloom Enabled", &m_bloomEnabled)) {
        r->SetBloomEnabled(m_bloomEnabled);
    }
    if (ImGui::SliderFloat("Bloom Threshold", &m_bloomThreshold, 0.1f, 3.0f)) {
        r->SetBloomThreshold(m_bloomThreshold);
    }
    if (ImGui::SliderFloat("Bloom Strength",  &m_bloomStrength,  0.0f, 0.5f)) {
        r->SetBloomStrength(m_bloomStrength);
    }
    if (ImGui::Checkbox("Show Bloom Buffer", &m_showBloomBuffer)) {
        r->SetShowBloomBuffer(m_showBloomBuffer);
    }
}

} // namespace ECS

#endif // USE_IMGUI
