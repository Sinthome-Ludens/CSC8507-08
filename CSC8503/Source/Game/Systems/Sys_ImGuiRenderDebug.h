/**
 * @file Sys_ImGuiRenderDebug.h
 * @brief 渲染调试面板系统声明：通过 ImGui 实时调节渲染管线参数。
 *
 * @details
 * 提供独立的 ImGui 调试窗口，允许在运行时调整：
 * - 线框模式
 * - CSM 级联分割距离 / PCSS 光源尺寸 / 调试级联可视化
 * - IBL 强度
 * - SSAO 开关 / 半径 / 偏置 / 显示缓冲
 * - Bloom 开关 / 阈值 / 强度 / 显示缓冲
 * - 曝光 / Vignette 强度
 *
 * 通过 `Res_NCL_Pointers::renderer` 动态转换为 `GameTechRenderer*` 后调用其 setter。
 *
 * 执行优先级：420（介于 Sys_Render=400 和 Sys_UI=500 之间）
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 渲染调试面板系统。
 * @details 通过 ImGui 窗口实时调节 GameTechRenderer 的渲染管线参数，无需重启。
 */
class Sys_ImGuiRenderDebug : public ISystem {
public:
    /**
     * @brief 获取 GameTechRenderer 指针并缓存；验证 Res_NCL_Pointers 存在。
     * @param registry 当前场景注册表
     */
    void OnAwake(Registry& registry) override;

    /**
     * @brief 渲染调试面板，将用户调整的参数传递给 GameTechRenderer。
     * @param registry 当前场景注册表
     * @param dt 本帧时间步长
     */
    void OnUpdate(Registry& registry, float dt) override;

    /**
     * @brief 清理系统，重置缓存的渲染器指针。
     * @param registry 当前场景注册表
     */
    void OnDestroy(Registry& registry) override;

private:
    /**
     * @brief 渲染"几何与线框"调试区块。
     * @param renderer GameTechRenderer 实例指针
     */
    void DrawGeometrySection(void* renderer);

    /**
     * @brief 渲染"阴影（CSM / PCSS）"调试区块。
     * @param renderer GameTechRenderer 实例指针
     */
    void DrawShadowSection(void* renderer);

    /**
     * @brief 渲染"IBL 环境光"调试区块。
     * @param renderer GameTechRenderer 实例指针
     */
    void DrawIBLSection(void* renderer);

    /**
     * @brief 渲染"SSAO"调试区块。
     * @param renderer GameTechRenderer 实例指针
     */
    void DrawSSAOSection(void* renderer);

    /**
     * @brief 渲染"后处理（Bloom / Tonemap / Vignette）"调试区块。
     * @param renderer GameTechRenderer 实例指针
     */
    void DrawPostProcessSection(void* renderer);

    // ── 当前面板参数（镜像 GameTechRenderer 状态，供 ImGui 读写）──────────
    bool  m_panelOpen        = true;
    bool  m_wireframe        = false;
    bool  m_debugCascades    = false;
    float m_cascadeSplits[3] = { 50.0f, 180.0f, 600.0f };
    float m_pcssLightSize    = 3.0f;
    float m_iblIntensity     = 1.0f;
    float m_shadowBiasSlope    = 0.00002f;
    float m_shadowBiasConstant = 0.000015f;
    bool  m_ssaoEnabled      = true;
    float m_ssaoRadius       = 0.5f;
    float m_ssaoBias         = 0.025f;
    bool  m_showSSAOBuffer   = false;
    bool  m_bloomEnabled     = true;
    float m_bloomThreshold   = 1.0f;
    float m_bloomStrength    = 0.04f;
    bool  m_showBloomBuffer  = false;
    float m_exposure         = 1.0f;
    float m_vignetteStrength = 0.3f;

    bool  m_showShadowMaps = false; ///< 显示级联 shadow map 预览

    float m_sunPos[3] = { -100.0f, 350.0f, -100.0f }; ///< 太阳位置（镜像 GameWorld::sunPosition）

    void* m_renderer  = nullptr; ///< GameTechRenderer*，在 OnAwake 中从 Res_NCL_Pointers 获取
    void* m_gameWorld = nullptr; ///< GameWorld*，用于 ImGui 调整太阳方向
};

} // namespace ECS

#endif // USE_IMGUI
