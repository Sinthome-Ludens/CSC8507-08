#include "Sys_PostProcess.h"
#include "Game/Components/Res_RenderTargets.h"
#include "Game/Components/Res_PostProcessConfig.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_PostProcess::OnAwake(Registry& registry) {
    // 确保 Res_PostProcessConfig 存在
    if (!registry.has_ctx<Res_PostProcessConfig>()) {
        registry.ctx_emplace<Res_PostProcessConfig>();
    }

    // 从 Res_RenderTargets 获取初始尺寸，初始化管线
    if (registry.has_ctx<Res_RenderTargets>()) {
        auto& rt = registry.ctx<Res_RenderTargets>();
        if (rt.width > 0 && rt.height > 0) {
            m_Pipeline.Init(rt.width, rt.height);
            m_LastWidth  = rt.width;
            m_LastHeight = rt.height;
            m_Initialized = true;
            LOG_INFO("[Sys_PostProcess] Initialized. Resolution: "
                     << rt.width << "x" << rt.height);
        } else {
            // 尺寸尚未填充（首帧前），延迟到 OnLateUpdate 初始化
            LOG_INFO("[Sys_PostProcess] Deferred init - RenderTargets not ready yet.");
        }
    } else {
        LOG_WARN("[Sys_PostProcess] Res_RenderTargets not registered.");
    }
}

void Sys_PostProcess::OnLateUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_RenderTargets>()) return;
    auto& rt = registry.ctx<Res_RenderTargets>();

    if (rt.hdrColorTex == 0 || rt.width == 0 || rt.height == 0) return;

    // 延迟初始化（OnAwake 时尺寸可能还未填充）
    if (!m_Initialized) {
        m_Pipeline.Init(rt.width, rt.height);
        m_LastWidth  = rt.width;
        m_LastHeight = rt.height;
        m_Initialized = true;
        LOG_INFO("[Sys_PostProcess] Late-initialized. Resolution: "
                 << rt.width << "x" << rt.height);
    }

    // 分辨率同步：每帧检测窗口尺寸变化
    if (rt.width != m_LastWidth || rt.height != m_LastHeight) {
        LOG_INFO("[Sys_PostProcess] Resize: "
                 << m_LastWidth << "x" << m_LastHeight
                 << " -> " << rt.width << "x" << rt.height);
        m_Pipeline.Resize(rt.width, rt.height);
        m_LastWidth  = rt.width;
        m_LastHeight = rt.height;
    }

    // 读取配置
    auto& config = registry.ctx<Res_PostProcessConfig>();

    PostProcessParams params;
    params.bloomThreshold  = config.bloomThreshold;
    params.bloomIntensity  = config.bloomIntensity;
    params.bloomIterations = config.bloomIterations;
    params.exposure        = config.exposure;
    params.gamma           = config.gamma;
    params.enableBloom     = config.enableBloom;
    params.enableTonemap   = config.enableTonemap;

    m_Pipeline.Execute(rt.hdrColorTex, params);
}

void Sys_PostProcess::OnDestroy(Registry& /*registry*/) {
    m_Pipeline.Destroy();
    m_Initialized = false;
    LOG_INFO("[Sys_PostProcess] Destroyed.");
}

} // namespace ECS
