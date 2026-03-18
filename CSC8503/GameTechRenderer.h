/**
 * @file GameTechRenderer.h
 * @brief 高级前向渲染器：HDR FBO、CSM 阴影、IBL、SSAO、后处理链、PBR 材质。
 *
 * @details
 * 渲染帧顺序：
 *   1. CSM Shadow Pass (3 cascades)
 *   2. Main Forward Pass → m_hdrFBO  (opaque → alpha-mask → transparent)
 *   3. Post-Processing  (SSAO → Bloom → Tonemap + FXAA → default FBO)
 *   4. Debug Overlay (lines, text, ImGui)
 */
#pragma once
#include "OGLRenderer.h"
#include "GameTechRendererInterface.h"
#include "OGLShader.h"

#include <array>
#include <vector>

namespace NCL {
    namespace Rendering {
        class OGLMesh;
        class OGLShader;
        class OGLTexture;
        class OGLBuffer;
    };
    namespace CSC8503 {
        class RenderObject;
        class GameWorld;

        class GameTechRenderer :
            public OGLRenderer,
            public NCL::CSC8503::GameTechRendererInterface
        {
        public:
            GameTechRenderer(GameWorld& world);
            ~GameTechRenderer();

            Mesh*    LoadMesh(const std::string& name)    override;
            Texture* LoadTexture(const std::string& name) override;

            void RenderScene()  { BeginFrame(); RenderFrame(); EndFrame(); }
            void PresentFrame() { SwapBuffers(); }
            void SetWireframeMode(bool enabled) override { m_wireframeMode = enabled; }

            /// @brief 窗口尺寸变化时重新分配所有屏幕尺寸相关 FBO 纹理（HDR / SSAO / PP）。
            void OnWindowResize(int w, int h) override;

            // ── ImGui 调试设置器 ─────────────────────────────
            void SetSSAOEnabled(bool v)           { m_ssaoEnabled = v; }
            void SetSSAORadius(float r)           { m_ssaoRadius  = r; }
            void SetSSAOBias(float b)             { m_ssaoBias    = b; }
            void SetBloomEnabled(bool v)          { m_bloomEnabled = v; }
            void SetBloomThreshold(float t)       { m_bloomThreshold = t; }
            void SetBloomStrength(float s)        { m_bloomStrength  = s; }
            void SetExposure(float e)             { m_exposure = e; }
            void SetVignetteStrength(float v)     { m_vignetteStrength = v; }
            void SetCascadeSplits(float n, float m, float f) {
                m_cascadeSplits[0] = n; m_cascadeSplits[1] = m; m_cascadeSplits[2] = f;
            }
            void SetPCSSLightSize(float s)        { m_pcssLightSize = s; }
            void SetIBLIntensity(float v)         { m_iblIntensity = v; }
            void SetDebugCascades(bool v)         { m_debugCascades = v; }
            void SetShowSSAOBuffer(bool v)        { m_showSSAOBuffer = v; }
            void SetShowBloomBuffer(bool v)       { m_showBloomBuffer = v; }
            /// @brief 设置 slope-scaled shadow bias，防止 shadow acne（单位：光照空间深度偏移量）
            void SetShadowBiasSlope(float v)      { m_shadowBiasSlope = v; }
            /// @brief 设置 constant shadow bias，防止 shadow acne（单位：光照空间深度偏移量）
            void SetShadowBiasConstant(float v)   { m_shadowBiasConstant = v; }
            /// @brief 设置 shadow near buffer（级联深度范围扩展，单位：世界空间）
            void SetShadowNearBuffer(float v)     { m_shadowNearBuffer = v; }
            /// @brief 设置 shadow far buffer（级联深度范围扩展，单位：世界空间）
            void SetShadowFarBuffer(float v)      { m_shadowFarBuffer = v; }
            /// @brief 设置 normal offset 缩放系数（sender-side bias，单位：texel 倍数）
            void SetShadowNormalOffsetScale(float v) { m_shadowNormalOffsetScale = v; }

            // ── Shadow debug getters ────────────────────────────
            static constexpr int GetNumCascades()       { return NUM_CASCADES; }
            GLuint GetShadowTex(int c) const            { return m_shadowTex[c]; }
            int    GetShadowRes(int c) const            { return m_shadowRes[c]; }
            const Matrix4& GetLightViewMat(int c) const { return m_lightViewMat[c]; }
            const Matrix4& GetLightProjMat(int c) const { return m_lightProjMat[c]; }

        protected:
            struct ObjectSortState {
                const RenderObject* object;
                float distanceFromCamera;
            };

            // ── 调试渲染 ─────────────────────────────────────
            void RenderLines();
            void RenderText();
            void RenderTextures();

            void RenderFrame() override;

            void BuildObjectLists();

            // ── 渲染 Pass ────────────────────────────────────
            void RenderSkyboxPass();
            void RenderShadowMapPass(std::vector<ObjectSortState>& list);
            void RenderOpaquePass(std::vector<ObjectSortState>& list);
            void RenderAlphaMaskPass(std::vector<ObjectSortState>& list);
            void RenderTransparentPass(std::vector<ObjectSortState>& list);
            void RenderPostProcessChain();

            // ── IBL / 工具 ──────────────────────────────────
            void GenerateIBL();
            void DrawFullscreenTriangle();
            void ComputeCascadeMatrices(const Matrix4& viewMatrix, const Matrix4& projMatrix);

            void LoadSkybox();
            void SetDebugStringBufferSizes(size_t newVertCount);
            void SetDebugLineBufferSizes(size_t newVertCount);

            // ── HDR FBO ─────────────────────────────────────
            GLuint m_hdrFBO       = 0;
            GLuint m_hdrColorTex  = 0; ///< RGBA16F — HDR 颜色
            GLuint m_hdrNormalTex = 0; ///< RGB16F  — 视空间法线（SSAO 用）
            GLuint m_hdrDepthTex  = 0; ///< DEPTH32F

            // ── CSM 阴影 ─────────────────────────────────────
            static constexpr int NUM_CASCADES = 3;
            GLuint  m_shadowTex[NUM_CASCADES] = {};
            GLuint  m_shadowFBO[NUM_CASCADES] = {};
            Matrix4 m_lightViewMat[NUM_CASCADES];
            Matrix4 m_lightProjMat[NUM_CASCADES];
            Matrix4 m_shadowMatrix[NUM_CASCADES];       ///< bias * proj[i] * lightView
            float   m_cascadeSplits[NUM_CASCADES]       = { 50.0f, 180.0f, 600.0f };
            int     m_shadowRes[NUM_CASCADES]            = { 4096, 2048, 1024 };
            float   m_shadowNormalOffset[NUM_CASCADES]   = {}; ///< 法线偏移量（半个 texel），消除接触阴影 light bleeding

            // ── IBL ──────────────────────────────────────────
            GLuint m_irradianceMap  = 0; ///< 32×32 立方体贴图（漫反射卷积）
            GLuint m_prefilterMap   = 0; ///< 128×128 立方体贴图，5 mip（粗糙度 LOD）
            GLuint m_brdfLUT        = 0; ///< 512×512 2D 纹理（split-sum LUT）
            bool   m_iblGenerated   = false;
            float  m_iblIntensity   = 1.0f;

            // ── SSAO ─────────────────────────────────────────
            GLuint m_ssaoFBO     = 0;
            GLuint m_ssaoBlurFBO = 0;
            GLuint m_ssaoTex     = 0; ///< R8
            GLuint m_ssaoBlurTex = 0; ///< R8
            GLuint m_ssaoNoiseTex = 0;
            std::vector<Vector3> m_ssaoKernel;
            bool  m_ssaoEnabled = true;
            float m_ssaoRadius  = 0.5f;
            float m_ssaoBias    = 0.025f;

            // ── Post-Process Ping-Pong ────────────────────────
            GLuint m_ppFBO[2] = {};
            GLuint m_ppTex[2] = {}; ///< RGBA16F × 2

            // ── Bloom ────────────────────────────────────────
            bool  m_bloomEnabled   = true;
            float m_bloomThreshold = 1.0f;
            float m_bloomStrength  = 0.04f;

            // ── Tone Mapping / Vignette ──────────────────────
            float m_exposure        = 1.0f;
            float m_vignetteStrength = 0.3f;

            // ── PCSS ─────────────────────────────────────────
            float m_pcssLightSize = 3.0f;

            // ── Shadow Bias ───────────────────────────────────
            float m_shadowBiasSlope    = 0.0001f;
            float m_shadowBiasConstant = 0.00005f;

            // ── Shadow Buffer & Normal Offset ─────────────────
            float m_shadowNearBuffer = 50.0f;
            float m_shadowFarBuffer  = 20.0f;
            float m_shadowNormalOffsetScale = 0.5f;

            // ── Debug flags ──────────────────────────────────
            bool  m_debugCascades   = false;
            bool  m_showSSAOBuffer  = false;
            bool  m_showBloomBuffer = false;
            bool  m_wireframeMode   = false;

            // ── 场景对象列表 ─────────────────────────────────
            std::vector<ObjectSortState> opaqueObjects;
            std::vector<ObjectSortState> transparentObjects;

            GameWorld& gameWorld;

            // ── 着色器 ───────────────────────────────────────
            OGLShader* defaultShader          = nullptr; ///< scene.vert + scene.frag (BlinnPhong)
            OGLShader* m_pbrShader            = nullptr; ///< lit.vert + lit_pbr.frag
            OGLShader* shadowShader           = nullptr; ///< shadow.vert + shadow.frag
            OGLShader* m_shadowAlphaTestShader= nullptr; ///< shadow.vert + shadow_alphatest.frag
            OGLShader* m_ssaoShader           = nullptr;
            OGLShader* m_ssaoBlurShader       = nullptr;
            OGLShader* m_bloomExtractShader   = nullptr;
            OGLShader* m_bloomBlurShader      = nullptr;
            OGLShader* m_bloomCompositeShader = nullptr;
            OGLShader* m_tonemapShader        = nullptr;
            OGLShader* m_fxaaShader           = nullptr;
            OGLShader* m_iblIrradianceShader  = nullptr;
            OGLShader* m_iblPrefilterShader   = nullptr;
            OGLShader* m_iblBrdfLutShader     = nullptr;

            // ── 天空盒 ───────────────────────────────────────
            OGLShader* skyboxShader = nullptr;
            OGLMesh*   skyboxMesh   = nullptr;
            GLuint     skyboxTex    = 0;

            // ── 调试渲染 ─────────────────────────────────────
            OGLShader* debugShader  = nullptr;
            OGLMesh*   debugTexMesh = nullptr;

            std::vector<Vector3> debugLineData;
            std::vector<Vector3> debugTextPos;
            std::vector<Vector4> debugTextColours;
            std::vector<Vector2> debugTextUVs;

            GLuint lineVAO      = 0;
            GLuint lineVertVBO  = 0;
            size_t lineCount    = 0;

            GLuint textVAO      = 0;
            GLuint textVertVBO  = 0;
            GLuint textColourVBO = 0;
            GLuint textTexVBO   = 0;
            size_t textCount    = 0;

            // ── 全屏三角形 VAO ───────────────────────────────
            GLuint m_fullscreenVAO = 0;

            // ── Fallback 纹理 ─────────────────────────────────
            GLuint m_fallbackWhiteTex  = 0; ///< 1×1 RGBA (255,255,255) — albedo 默认白色
            GLuint m_fallbackNormalTex = 0; ///< 1×1 RGBA (128,128,255) — 中性法线 (0,0,1)
            GLuint m_fallbackOrmTex    = 0; ///< 1×1 RGBA (255,128,0)   — AO=1, roughness=0.5, metallic=0
            GLuint m_fallbackBlackTex  = 0; ///< 1×1 RGBA (0,0,0)       — emissive 默认黑色

            // ── 辅助绘制方法 ─────────────────────────────────
            void DrawObject(OGLShader* shader, const RenderObject* o);
        };
    }
}
