/**
 * @file GameTechRenderer.cpp
 * @brief 高级前向渲染器实现：CSM 阴影、PBR/BlinnPhong 材质分支、SSAO、Bloom、ACES Tone Map、FXAA。
 *
 * @details
 * 帧渲染顺序：
 *   1. ComputeCascadeMatrices — 根据相机视锥拟合正交 CSM 投影
 *   2. RenderShadowMapPass   — 3 个级联深度图（4096/2048/1024）
 *   3. 绑定 m_hdrFBO
 *   4. RenderSkyboxPass
 *   5. RenderOpaquePass      — PBR/BlinnPhong，写 hdrColor + gNormal
 *   6. RenderAlphaMaskPass   — Alpha Mask（discard），同样写 hdrColor
 *   7. RenderTransparentPass — 半透明混合
 *   8. RenderPostProcessChain
 *        a. SSAO + SSAO Blur
 *        b. Bloom Extract → Kawase Blur × 5 → Composite
 *        c. Tonemap（ACES + Gamma）+ Vignette → ppTex[0]
 *        d. FXAA → default FBO 0
 *   9. Debug Overlay（lines、textures、text）
 */

#include "GameTechRenderer.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "RenderObject.h"
#include "Camera.h"
#include "TextureLoader.h"
#include "MshLoader.h"

#include "Debug.h"

#include "OGLRenderer.h"
#include "OGLShader.h"
#include "OGLTexture.h"
#include "OGLMesh.h"
#include "Matrix.h"
#include "Vector.h"

#include <random>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>

using namespace NCL;
using namespace Rendering;
using namespace CSC8503;

static const Matrix4 biasMatrix =
    Matrix::Translation(Vector3(0.5f, 0.5f, 0.5f)) *
    Matrix::Scale(Vector3(0.5f, 0.5f, 0.5f));

// ============================================================
// 构造 / 析构
// ============================================================

GameTechRenderer::GameTechRenderer(GameWorld& world)
    : OGLRenderer(*Window::GetWindow()), gameWorld(world)
{
    glEnable(GL_DEPTH_TEST);

    // ── 基础着色器 ─────────────────────────────────────
    debugShader           = new OGLShader("debug.vert",    "debug.frag");
    shadowShader          = new OGLShader("shadow.vert",   "shadow.frag");
    defaultShader         = new OGLShader("scene.vert",    "scene.frag");
    m_pbrShader           = new OGLShader("lit.vert",      "lit_pbr.frag");
    m_shadowAlphaTestShader = new OGLShader("shadow.vert", "shadow_alphatest.frag");
    m_ssaoShader          = new OGLShader("fullscreen.vert", "ssao.frag");
    m_ssaoBlurShader      = new OGLShader("fullscreen.vert", "ssao_blur.frag");
    m_bloomExtractShader  = new OGLShader("fullscreen.vert", "bloom_extract.frag");
    m_bloomBlurShader     = new OGLShader("fullscreen.vert", "bloom_blur.frag");
    m_bloomCompositeShader= new OGLShader("fullscreen.vert", "bloom_composite.frag");
    m_tonemapShader       = new OGLShader("fullscreen.vert", "tonemap.frag");
    m_fxaaShader          = new OGLShader("fullscreen.vert", "fxaa.frag");
    m_iblIrradianceShader = new OGLShader("fullscreen.vert", "ibl_irradiance.frag");
    m_iblPrefilterShader  = new OGLShader("fullscreen.vert", "ibl_prefilter.frag");
    m_iblBrdfLutShader    = new OGLShader("fullscreen.vert", "ibl_brdf_lut.frag");

    // ── CSM 深度纹理（不设 compare mode，PCSS 需要读原始深度）────
    for (int c = 0; c < NUM_CASCADES; c++) {
        glGenTextures(1, &m_shadowTex[c]);
        glBindTexture(GL_TEXTURE_2D, m_shadowTex[c]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
                     m_shadowRes[c], m_shadowRes[c], 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        glGenFramebuffers(1, &m_shadowFBO[c]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO[c]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowTex[c], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        {
            GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (s != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "[Renderer] Shadow FBO " << c << " incomplete: 0x" << std::hex << s << std::dec << "\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // ── HDR FBO ───────────────────────────────────────
    {
        const int w = (int)windowSize.x;
        const int h = (int)windowSize.y;

        glGenTextures(1, &m_hdrColorTex);
        glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &m_hdrNormalTex);
        glBindTexture(GL_TEXTURE_2D, m_hdrNormalTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &m_hdrDepthTex);
        glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_hdrFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_hdrColorTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_hdrNormalTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_TEXTURE_2D, m_hdrDepthTex, 0);
        GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, drawBuffers);
        {
            GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (s != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "[Renderer] HDR FBO incomplete: 0x" << std::hex << s << std::dec << "\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // ── SSAO FBO + 噪声纹理 + 采样核 ────────────────────
    {
        const int w = (int)windowSize.x;
        const int h = (int)windowSize.y;

        glGenTextures(1, &m_ssaoTex);
        glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &m_ssaoFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoTex, 0);
        {
            GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (s != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "[Renderer] SSAO FBO incomplete: 0x" << std::hex << s << std::dec << "\n";
        }

        glGenTextures(1, &m_ssaoBlurTex);
        glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1, &m_ssaoBlurFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoBlurTex, 0);
        {
            GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (s != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "[Renderer] SSAO Blur FBO incomplete: 0x" << std::hex << s << std::dec << "\n";
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // 4×4 旋转噪声纹理
        std::uniform_real_distribution<float> rng(0.0f, 1.0f);
        std::default_random_engine gen;
        float noiseData[16 * 3];
        for (int i = 0; i < 16; i++) {
            noiseData[i * 3 + 0] = rng(gen) * 2.0f - 1.0f;
            noiseData[i * 3 + 1] = rng(gen) * 2.0f - 1.0f;
            noiseData[i * 3 + 2] = 0.0f;
        }
        glGenTextures(1, &m_ssaoNoiseTex);
        glBindTexture(GL_TEXTURE_2D, m_ssaoNoiseTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noiseData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        // 64 个半球采样核
        m_ssaoKernel.resize(64);
        for (int i = 0; i < 64; i++) {
            Vector3 s(rng(gen) * 2.0f - 1.0f, rng(gen) * 2.0f - 1.0f, rng(gen));
            s = Vector::Normalise(s);
            s = s * rng(gen);
            float scale = (float)i / 64.0f;
            scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale^2)
            m_ssaoKernel[i] = s * scale;
        }
    }

    // ── Post-Process Ping-Pong FBO ───────────────────
    {
        const int w = (int)windowSize.x;
        const int h = (int)windowSize.y;
        for (int i = 0; i < 2; i++) {
            glGenTextures(1, &m_ppTex[i]);
            glBindTexture(GL_TEXTURE_2D, m_ppTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenFramebuffers(1, &m_ppFBO[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, m_ppFBO[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ppTex[i], 0);
            {
                GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                if (s != GL_FRAMEBUFFER_COMPLETE)
                    std::cout << "[Renderer] PP FBO " << i << " incomplete: 0x" << std::hex << s << std::dec << "\n";
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // ── IBL 目标纹理（延迟到 GenerateIBL() 首帧填充）──────
    {
        // Irradiance: 32×32 cubemap
        glGenTextures(1, &m_irradianceMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
        for (int i = 0; i < 6; i++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                         32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Prefilter: 128×128 cubemap with 5 mip levels
        glGenTextures(1, &m_prefilterMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
        for (int i = 0; i < 6; i++)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                         128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        // BRDF LUT: 512×512 RG16F
        glGenTextures(1, &m_brdfLUT);
        glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

    glClearColor(0.961f, 0.933f, 0.910f, 1.0f); // #F5EEE8

    // ── 天空盒 ────────────────────────────────────────
    skyboxShader = new OGLShader("skybox.vert", "skybox.frag");
    skyboxMesh   = new OGLMesh();
    skyboxMesh->SetVertexPositions({
        Vector3(-1,  1, -1), Vector3(-1, -1, -1),
        Vector3( 1, -1, -1), Vector3( 1,  1, -1)
    });
    skyboxMesh->SetVertexIndices({ 0, 1, 2, 2, 3, 0 });
    skyboxMesh->UploadToGPU();
    LoadSkybox();

    // ── 调试 VAO/VBO ─────────────────────────────────
    glGenVertexArrays(1, &lineVAO);
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &lineVertVBO);
    glGenBuffers(1, &textVertVBO);
    glGenBuffers(1, &textColourVBO);
    glGenBuffers(1, &textTexVBO);

    Debug::CreateDebugFont("PressStart2P.fnt", *LoadTexture("PressStart2P.png"));

    debugTexMesh = new OGLMesh();
    debugTexMesh->SetVertexPositions({
        Vector3(-1,  1, 0), Vector3(-1, -1, 0),
        Vector3( 1, -1, 0), Vector3( 1,  1, 0)
    });
    debugTexMesh->SetVertexTextureCoords({
        Vector2(0, 1), Vector2(0, 0), Vector2(1, 0), Vector2(1, 1)
    });
    debugTexMesh->SetVertexIndices({ 0, 1, 2, 2, 3, 0 });
    debugTexMesh->UploadToGPU();

    // ── 全屏三角形 VAO（无 VBO，仅需 gl_VertexID）────────
    glGenVertexArrays(1, &m_fullscreenVAO);

    // ── 1×1 语义 fallback 纹理 ────────────────────────
    {
        auto makeFallback = [](GLuint& tex, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
            unsigned char px[4] = { r, g, b, a };
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0);
        };
        makeFallback(m_fallbackWhiteTex,  255, 255, 255, 255); // albedo: 白色
        makeFallback(m_fallbackNormalTex, 128, 128, 255, 255); // normal: 中性法线 (0,0,1) in tangent space
        makeFallback(m_fallbackOrmTex,    255, 128,   0, 255); // ORM: AO=1.0, roughness=0.5, metallic=0.0
        makeFallback(m_fallbackBlackTex,    0,   0,   0, 255); // emissive: 黑色（无自发光）
    }

    SetDebugStringBufferSizes(10000);
    SetDebugLineBufferSizes(1000);

    // ── Instanced Rendering SSBO ─────────────────────────
    glGenBuffers(1, &m_instanceSSBO);
    m_instanceSSBOSize = 0;

    // ── Ocean SSBO（binding point 1）─────────────────────
    glGenBuffers(1, &m_oceanSSBO);
    m_oceanSSBOSize = 0;
}

GameTechRenderer::~GameTechRenderer() {
    for (int c = 0; c < NUM_CASCADES; c++) {
        glDeleteTextures(1, &m_shadowTex[c]);
        glDeleteFramebuffers(1, &m_shadowFBO[c]);
    }
    glDeleteTextures(1, &m_hdrColorTex);
    glDeleteTextures(1, &m_hdrNormalTex);
    glDeleteTextures(1, &m_hdrDepthTex);
    glDeleteFramebuffers(1, &m_hdrFBO);
    glDeleteTextures(1, &m_ssaoTex);
    glDeleteTextures(1, &m_ssaoBlurTex);
    glDeleteTextures(1, &m_ssaoNoiseTex);
    glDeleteFramebuffers(1, &m_ssaoFBO);
    glDeleteFramebuffers(1, &m_ssaoBlurFBO);
    for (int i = 0; i < 2; i++) {
        glDeleteTextures(1, &m_ppTex[i]);
        glDeleteFramebuffers(1, &m_ppFBO[i]);
    }
    glDeleteTextures(1, &m_irradianceMap);
    glDeleteTextures(1, &m_prefilterMap);
    glDeleteTextures(1, &m_brdfLUT);
    glDeleteTextures(1, &m_fallbackWhiteTex);
    glDeleteTextures(1, &m_fallbackNormalTex);
    glDeleteTextures(1, &m_fallbackOrmTex);
    glDeleteTextures(1, &m_fallbackBlackTex);
    glDeleteVertexArrays(1, &m_fullscreenVAO);
    if (m_instanceSSBO) glDeleteBuffers(1, &m_instanceSSBO);
    if (m_oceanSSBO) glDeleteBuffers(1, &m_oceanSSBO);
}

void GameTechRenderer::LoadSkybox() {
    std::string filenames[6] = {
        "/Cubemap/skyrender0004.png",
        "/Cubemap/skyrender0001.png",
        "/Cubemap/skyrender0003.png",
        "/Cubemap/skyrender0006.png",
        "/Cubemap/skyrender0002.png",
        "/Cubemap/skyrender0005.png"
    };

    uint32_t width[6] = {}, height[6] = {}, channels[6] = {}, flags[6] = {};
    vector<char*> texData(6, nullptr);

    for (int i = 0; i < 6; ++i) {
        TextureLoader::LoadTexture(filenames[i], texData[i], width[i], height[i], channels[i], flags[i]);
        if (i > 0 && (width[i] != width[0] || height[i] != height[0])) {
            std::cout << __FUNCTION__ << " cubemap input textures don't match in size?\n";
            return;
        }
    }

    glGenTextures(1, &skyboxTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);
    GLenum type = channels[0] == 4 ? GL_RGBA : GL_RGB;
    for (int i = 0; i < 6; ++i)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB,
                     width[i], height[i], 0, type, GL_UNSIGNED_BYTE, texData[i]);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

Mesh* GameTechRenderer::LoadMesh(const std::string& name) {
    OGLMesh* mesh = new OGLMesh();
    MshLoader::LoadMesh(name, *mesh);
    mesh->SetPrimitiveType(GeometryPrimitive::Triangles);
    mesh->UploadToGPU();
    return mesh;
}

Texture* GameTechRenderer::LoadTexture(const std::string& name) {
    return OGLTexture::TextureFromFile(name).release();
}

// ============================================================
// OnWindowResize — 重新分配所有屏幕尺寸 FBO 纹理
// ============================================================

void GameTechRenderer::OnWindowResize(int w, int h) {
    OGLRenderer::OnWindowResize(w, h);

    if (w <= 0 || h <= 0) return;

    glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, m_hdrNormalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, m_ppTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

// ============================================================
// DrawFullscreenTriangle
// ============================================================

void GameTechRenderer::DrawFullscreenTriangle() {
    glBindVertexArray(m_fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

// ============================================================
// GenerateIBL — 首帧调用，离线卷积天空盒生成 IBL 纹理
// ============================================================

void GameTechRenderer::GenerateIBL() {
    if (m_iblGenerated || !skyboxTex) return;

    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    // ── BRDF LUT ──────────────────────────────────────
    if (m_iblBrdfLutShader) {
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
        glViewport(0, 0, 512, 512);
        UseShader(*m_iblBrdfLutShader);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DrawFullscreenTriangle();
    }

    // ── Irradiance Map（6 faces × 32×32）─────────────
    if (m_iblIrradianceShader) {
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
        UseShader(*m_iblIrradianceShader);
        int skyLoc = glGetUniformLocation(m_iblIrradianceShader->GetProgramID(), "environmentMap");
        glUniform1i(skyLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);
        glViewport(0, 0, 32, 32);
        for (int face = 0; face < 6; face++) {
            int faceLoc = glGetUniformLocation(m_iblIrradianceShader->GetProgramID(), "face");
            glUniform1i(faceLoc, face);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_irradianceMap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawFullscreenTriangle();
        }
    }

    // ── Prefilter Map（6 faces × 5 mip levels）────────
    if (m_iblPrefilterShader) {
        UseShader(*m_iblPrefilterShader);
        int skyLoc  = glGetUniformLocation(m_iblPrefilterShader->GetProgramID(), "environmentMap");
        int roughLoc = glGetUniformLocation(m_iblPrefilterShader->GetProgramID(), "roughness");
        int faceLoc = glGetUniformLocation(m_iblPrefilterShader->GetProgramID(), "face");
        glUniform1i(skyLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);

        for (int mip = 0; mip < 5; mip++) {
            int mipSize = 128 >> mip;
            glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);
            glViewport(0, 0, mipSize, mipSize);
            float roughness = (float)mip / 4.0f;
            glUniform1f(roughLoc, roughness);
            for (int face = 0; face < 6; face++) {
                glUniform1i(faceLoc, face);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_prefilterMap, mip);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                DrawFullscreenTriangle();
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);
    glViewport(0, 0, (GLsizei)windowSize.x, (GLsizei)windowSize.y);

    m_iblGenerated = true;
}

// ============================================================
// ComputeCascadeMatrices
// ============================================================

void GameTechRenderer::ComputeCascadeMatrices(const Matrix4& viewMatrix, const Matrix4& projMatrix) {
    Vector3 sunPos = gameWorld.GetSunPosition();
    Vector3 lightDir(-sunPos.x, -sunPos.y, -sunPos.z);
    float dirLen = sqrtf(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
    if (dirLen > 0.001f) { lightDir.x /= dirLen; lightDir.y /= dirLen; lightDir.z /= dirLen; }
    Vector3 lightUp(0, 1, 0);
    if (fabsf(lightDir.y) > 0.99f) lightUp = Vector3(0, 0, 1);

    float tanFovV = 1.0f / projMatrix.array[1][1];
    float aspect  = hostWindow.GetScreenAspect();
    float tanFovH = tanFovV * aspect;

    float nearPrev = 0.1f;
    for (int c = 0; c < NUM_CASCADES; c++) {
        float farDist = m_cascadeSplits[c];

        Vector3 camPos   = gameWorld.GetMainCamera().GetPosition();
        Vector3 camFwd   = Vector3(-viewMatrix.array[0][2], -viewMatrix.array[1][2], -viewMatrix.array[2][2]);
        Vector3 camRight = Vector3( viewMatrix.array[0][0],  viewMatrix.array[1][0],  viewMatrix.array[2][0]);
        Vector3 camUp    = Vector3( viewMatrix.array[0][1],  viewMatrix.array[1][1],  viewMatrix.array[2][1]);

        float nearW = nearPrev * tanFovH, nearH = nearPrev * tanFovV;
        float farW  = farDist  * tanFovH, farH  = farDist  * tanFovV;

        Vector3 corners[8];
        corners[0] = camPos + camFwd * nearPrev - camRight * nearW - camUp * nearH;
        corners[1] = camPos + camFwd * nearPrev + camRight * nearW - camUp * nearH;
        corners[2] = camPos + camFwd * nearPrev - camRight * nearW + camUp * nearH;
        corners[3] = camPos + camFwd * nearPrev + camRight * nearW + camUp * nearH;
        corners[4] = camPos + camFwd * farDist  - camRight * farW  - camUp * farH;
        corners[5] = camPos + camFwd * farDist  + camRight * farW  - camUp * farH;
        corners[6] = camPos + camFwd * farDist  - camRight * farW  + camUp * farH;
        corners[7] = camPos + camFwd * farDist  + camRight * farW  + camUp * farH;

        Vector3 frustumCenter(0, 0, 0);
        for (int i = 0; i < 8; i++) {
            frustumCenter.x += corners[i].x;
            frustumCenter.y += corners[i].y;
            frustumCenter.z += corners[i].z;
        }
        frustumCenter.x /= 8.0f;
        frustumCenter.y /= 8.0f;
        frustumCenter.z /= 8.0f;

        float pullBack = farDist + 200.0f;
        Vector3 lightEye(
            frustumCenter.x - lightDir.x * pullBack,
            frustumCenter.y - lightDir.y * pullBack,
            frustumCenter.z - lightDir.z * pullBack);
        m_lightViewMat[c] = Matrix::View(lightEye, frustumCenter, lightUp);

        Vector3 minLS(1e9f, 1e9f, 1e9f), maxLS(-1e9f, -1e9f, -1e9f);
        for (int i = 0; i < 8; i++) {
            Vector4 lsPos = m_lightViewMat[c] * Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
            Vector3 ls(lsPos.x, lsPos.y, lsPos.z);
            minLS.x = std::min(minLS.x, ls.x); minLS.y = std::min(minLS.y, ls.y); minLS.z = std::min(minLS.z, ls.z);
            maxLS.x = std::max(maxLS.x, ls.x); maxLS.y = std::max(maxLS.y, ls.y); maxLS.z = std::max(maxLS.z, ls.z);
        }

        float rangeX   = maxLS.x - minLS.x;
        float rangeY   = maxLS.y - minLS.y;
        float maxRange = std::max(rangeX, rangeY);
        float texelSize = maxRange / (float)m_shadowRes[c];
        m_shadowNormalOffset[c] = texelSize * m_shadowNormalOffsetScale;

        float centerX = std::round(((minLS.x + maxLS.x) * 0.5f) / texelSize) * texelSize;
        float centerY = std::round(((minLS.y + maxLS.y) * 0.5f) / texelSize) * texelSize;
        float halfExt = maxRange * 0.5f;
        minLS.x = centerX - halfExt;
        maxLS.x = centerX + halfExt;
        minLS.y = centerY - halfExt;
        maxLS.y = centerY + halfExt;

        float shadowNear = std::max(0.1f, -(maxLS.z + m_shadowNearBuffer));
        float shadowFar  = -(minLS.z - m_shadowFarBuffer);
        if (shadowFar <= shadowNear) shadowFar = shadowNear + 100.0f;
        m_lightProjMat[c] = Matrix::Orthographic(minLS.x, maxLS.x,
                                                  minLS.y, maxLS.y,
                                                  shadowNear, shadowFar);
        m_shadowMatrix[c] = biasMatrix * m_lightProjMat[c] * m_lightViewMat[c];

        nearPrev = farDist;
    }
}

// ============================================================
// Ocean SSBO — 数据海洋 GPU 驱动噪波
// ============================================================

void GameTechRenderer::SetOceanPillarProxies(const std::vector<GameObject*>* proxies) {
    m_oceanPillarProxiesPtr = proxies;
    m_oceanSSBOReady = false;
}

void GameTechRenderer::SetOceanTime(float time) {
    m_oceanTime = time;
}

void GameTechRenderer::SetOceanNoiseParams(float scale, float speed, float amplitude) {
    m_oceanNoiseScale = scale;
    m_oceanNoiseSpeed = speed;
    m_oceanBaseAmplitude = amplitude;
}

/**
 * @brief 一次性上传柱子静态数据到 ocean SSBO（binding point 1）。
 * @details OceanInstanceData 布局：mat4 modelMatrix + vec4 objectColour + vec4 pillarParams。
 *          后续帧只需 bind + 更新 oceanTime uniform，无需重新上传。
 */
void GameTechRenderer::UploadOceanSSBO() {
    if (!m_oceanPillarProxiesPtr || m_oceanPillarProxiesPtr->empty()) return;

    struct OceanInstanceData {
        Matrix4 modelMatrix;
        Vector4 objectColour;
        Vector4 pillarParams;
    };

    int count = (int)m_oceanPillarProxiesPtr->size();
    size_t requiredSize = sizeof(OceanInstanceData) * count;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_oceanSSBO);
    if (requiredSize > m_oceanSSBOSize) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, requiredSize, nullptr, GL_STATIC_DRAW);
        m_oceanSSBOSize = requiredSize;
    }

    OceanInstanceData* ptr = (OceanInstanceData*)glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0, requiredSize,
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr) {
        for (int i = 0; i < count; ++i) {
            auto* proxy = (*m_oceanPillarProxiesPtr)[i];
            const auto* ro = proxy->GetRenderObject();
            ptr[i].modelMatrix  = proxy->GetTransform().GetMatrix();
            ptr[i].objectColour = ro ? ro->GetColour() : Vector4(1, 1, 1, 1);
            ptr[i].pillarParams = Vector4(
                ro ? ro->pillarBaseY : 0.0f,
                ro ? ro->pillarAmplitude : 2.0f,
                ro ? ro->pillarPhaseShift : 0.0f,
                0.0f
            );
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    m_oceanPillarCount = count;
    m_oceanSSBOReady = true;
}

/**
 * @brief 使用 ocean SSBO + GPU 噪波绘制柱子（forward pass）。
 */
void GameTechRenderer::DrawOceanBatch(OGLShader* shader,
                                       const Matrix4& viewMatrix, const Matrix4& projMatrix)
{
    if (!m_oceanSSBOReady || m_oceanPillarCount == 0 || !m_oceanPillarProxiesPtr) return;

    GLuint pid = shader->GetProgramID();
    auto ul = [&](const char* n) { return glGetUniformLocation(pid, n); };

    glUniformMatrix4fv(ul("viewMatrix"), 1, false, (float*)&viewMatrix);
    glUniformMatrix4fv(ul("projMatrix"), 1, false, (float*)&projMatrix);
    glUniform1i(ul("useSkinning"), 0);
    glUniform1i(ul("useInstancing"), 0);
    glUniform1i(ul("useOceanNoise"), 1);
    glUniform1f(ul("oceanTime"), m_oceanTime);
    glUniform1f(ul("oceanNoiseScale"), m_oceanNoiseScale);
    glUniform1f(ul("oceanNoiseSpeed"), m_oceanNoiseSpeed);
    glUniform1f(ul("oceanBaseAmplitude"), m_oceanBaseAmplitude);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_oceanSSBO);

    auto* firstProxy = (*m_oceanPillarProxiesPtr)[0];
    const auto* ro = firstProxy->GetRenderObject();
    if (!ro) return;

    const GameTechMaterial& mat = ro->GetMaterial();

    auto bindTex = [&](Texture* tex, const char* samplerName, int unit, GLuint fallbackID) {
        glActiveTexture(GL_TEXTURE0 + unit);
        if (tex) {
            OGLTexture* t = (OGLTexture*)tex;
            glBindTexture(GL_TEXTURE_2D, t->GetObjectID());
        } else {
            glBindTexture(GL_TEXTURE_2D, fallbackID);
        }
        glUniform1i(ul(samplerName), unit);
    };

    bindTex(mat.diffuseTex,  "albedoTex",   0, m_fallbackWhiteTex);
    bindTex(mat.bumpTex,     "normalTex",   1, m_fallbackNormalTex);
    bindTex(mat.ormTex,      "ormTex",      2, m_fallbackOrmTex);
    bindTex(mat.emissiveTex, "emissiveTex", 3, m_fallbackBlackTex);

    glUniform1i(ul("hasTexture"),     mat.diffuseTex  ? 1 : 0);
    glUniform1i(ul("hasBumpTex"),     mat.bumpTex     ? 1 : 0);
    glUniform1i(ul("hasOrmTex"),      mat.ormTex      ? 1 : 0);
    glUniform1i(ul("hasEmissiveTex"), mat.emissiveTex ? 1 : 0);
    glUniform1i(ul("hasVertexColours"), 0);

    glUniform1f(ul("metallic"),          mat.metallic);
    glUniform1f(ul("roughness"),         mat.roughness);
    glUniform1f(ul("ao"),                mat.ao);
    glUniform3fv(ul("emissiveColor"),    1, (float*)&mat.emissiveColor);
    glUniform1f(ul("emissiveStrength"),  mat.emissiveStrength);
    glUniform3fv(ul("rimColour"),        1, (float*)&mat.rimColour);
    glUniform1f(ul("rimPower"),          mat.rimPower);
    glUniform1f(ul("rimStrength"),       mat.rimStrength);
    glUniform1i(ul("alphaMode"),    0);
    glUniform1f(ul("alphaCutoff"),  0.5f);
    glUniform1i(ul("doubleSided"),  0);

    OGLMesh* mesh = (OGLMesh*)ro->GetMesh();
    BindMesh(*mesh);
    size_t layerCount = mesh->GetSubMeshCount();
    if (layerCount == 0) DrawBoundMesh(0, m_oceanPillarCount);
    else for (size_t i = 0; i < layerCount; i++) DrawBoundMesh((uint32_t)i, m_oceanPillarCount);

    glUniform1i(ul("useOceanNoise"), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
}

/**
 * @brief 使用 ocean SSBO + GPU 噪波绘制柱子阴影。
 */
void GameTechRenderer::DrawOceanShadowBatch(OGLShader* shader,
                                             const Matrix4& lightVP, float normalOffsetBias)
{
    if (!m_oceanSSBOReady || m_oceanPillarCount == 0 || !m_oceanPillarProxiesPtr) return;

    auto* firstProxy = (*m_oceanPillarProxiesPtr)[0];
    const auto* ro = firstProxy->GetRenderObject();
    if (!ro) return;

    UseShader(*shader);
    GLuint pid = shader->GetProgramID();

    glUniform1i(glGetUniformLocation(pid, "useModelMatrix"), 0);
    glUniform1i(glGetUniformLocation(pid, "useInstancing"), 0);
    glUniform1i(glGetUniformLocation(pid, "useOceanNoise"), 1);
    glUniformMatrix4fv(glGetUniformLocation(pid, "lightVPMatrix"), 1, false, (float*)&lightVP);
    glUniform1f(glGetUniformLocation(pid, "normalOffsetBias"), normalOffsetBias);
    glUniform1i(glGetUniformLocation(pid, "useSkinning"), 0);
    glUniform1f(glGetUniformLocation(pid, "oceanTime"), m_oceanTime);
    glUniform1f(glGetUniformLocation(pid, "oceanNoiseScale"), m_oceanNoiseScale);
    glUniform1f(glGetUniformLocation(pid, "oceanNoiseSpeed"), m_oceanNoiseSpeed);
    glUniform1f(glGetUniformLocation(pid, "oceanBaseAmplitude"), m_oceanBaseAmplitude);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_oceanSSBO);

    OGLMesh* mesh = (OGLMesh*)ro->GetMesh();
    BindMesh(*mesh);
    size_t layerCount = mesh->GetSubMeshCount();
    if (layerCount == 0) DrawBoundMesh(0, m_oceanPillarCount);
    else for (size_t j = 0; j < layerCount; j++) DrawBoundMesh((uint32_t)j, m_oceanPillarCount);

    glUniform1i(glGetUniformLocation(pid, "useOceanNoise"), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
}

// ============================================================
// RenderFrame
// ============================================================

void GameTechRenderer::RenderFrame() {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    BuildObjectLists();

    if (!m_oceanSSBOReady && m_oceanPillarProxiesPtr && !m_oceanPillarProxiesPtr->empty()) {
        UploadOceanSSBO();
    }

    if (!m_iblGenerated) GenerateIBL();

    Matrix4 viewMatrix = gameWorld.GetMainCamera().BuildViewMatrix();
    Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());

    // ── 1. CSM Shadow Pass ────────────────────────────
    ComputeCascadeMatrices(viewMatrix, projMatrix);
    if (!opaqueObjects.empty() || !transparentObjects.empty()) {
        OGLDebugScope scope("CSM Shadow Pass");
        RenderShadowMapPass(opaqueObjects);
    }

    // ── 2. Main Forward Pass → HDR FBO ───────────────
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glViewport(0, 0, (GLsizei)windowSize.x, (GLsizei)windowSize.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    {
        OGLDebugScope scope("Skybox Pass");
        RenderSkyboxPass();
    }
    {
        OGLDebugScope scope("Opaque Pass");
        RenderOpaquePass(opaqueObjects);
    }
    {
        OGLDebugScope scope("Alpha Mask Pass");
        RenderAlphaMaskPass(opaqueObjects);
    }
    {
        OGLDebugScope scope("Transparent Pass");
        RenderTransparentPass(transparentObjects);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ── 3. Post-Process Chain ────────────────────────
    {
        OGLDebugScope scope("Post Process");
        RenderPostProcessChain();
    }

    // ── 4. Debug Overlay (直接写 default FBO) ────────
    {
        OGLDebugScope scope("Debug Overlay");
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        RenderLines();
        RenderTextures();
        RenderText();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}

// ============================================================
// BuildObjectLists
// ============================================================

void GameTechRenderer::BuildObjectLists() {
    opaqueObjects.clear();
    transparentObjects.clear();
    m_instancedBatches.clear();

    Vector3 camPos = gameWorld.GetMainCamera().GetPosition();

    /// @brief 临时 map：(mesh ptr, shader ptr) → batch index
    std::unordered_map<uint64_t, size_t> batchMap;

    gameWorld.OperateOnContents([&](GameObject* o) {
        if (o->IsActive()) {
            const RenderObject* g = o->GetRenderObject();
            if (g) {
                g->instanced = false;
                const GameTechMaterial& mat = g->GetMaterial();
                ObjectSortState s;
                s.object = g;
                s.distanceFromCamera = Vector::LengthSquared(camPos - g->GetTransform().GetPosition());

                if (mat.type == MaterialType::Transparent) {
                    transparentObjects.emplace_back(s);
                } else {
                    opaqueObjects.emplace_back(s);

                    if (mat.alphaMode == AlphaMode::Opaque && !g->useSkinning) {
                        OGLMesh* mesh = (OGLMesh*)g->GetMesh();
                        OGLShader* shader = (mat.shadingModel == ShadingModel::PBR) ? m_pbrShader : defaultShader;
                        uint64_t key = (uint64_t)(uintptr_t)mesh ^ ((uint64_t)(uintptr_t)shader << 32);
                        auto it = batchMap.find(key);
                        if (it == batchMap.end()) {
                            batchMap[key] = m_instancedBatches.size();
                            InstancedBatch batch;
                            batch.mesh   = mesh;
                            batch.shader = shader;
                            batch.objects.push_back(g);
                            m_instancedBatches.push_back(std::move(batch));
                        } else {
                            m_instancedBatches[it->second].objects.push_back(g);
                        }
                    }
                }
            }
        }
    });

    for (const auto& batch : m_instancedBatches) {
        if ((int)batch.objects.size() >= INSTANCE_THRESHOLD) {
            for (const auto* obj : batch.objects)
                obj->instanced = true;
        }
    }

    for (auto& batch : m_instancedBatches) {
        if (!batch.objects.empty()) {
            batch.shadowCascadeMask = batch.objects[0]->shadowCascadeMask;
        }
    }

    opaqueObjects.erase(
        std::remove_if(opaqueObjects.begin(), opaqueObjects.end(),
            [](const ObjectSortState& s) { return s.object->instanced; }),
        opaqueObjects.end());

    std::sort(opaqueObjects.begin(), opaqueObjects.end(),
        [](const ObjectSortState& a, const ObjectSortState& b) {
            return a.distanceFromCamera < b.distanceFromCamera;
        });
    std::sort(transparentObjects.rbegin(), transparentObjects.rend(),
        [](const ObjectSortState& a, const ObjectSortState& b) {
            return a.distanceFromCamera < b.distanceFromCamera;
        });
}

// ============================================================
// RenderShadowMapPass — 3 CSM 级联
// ============================================================

void GameTechRenderer::RenderShadowMapPass(std::vector<ObjectSortState>& list) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glCullFace(GL_BACK);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    struct ShadowInstanceData {
        Matrix4 modelMatrix;
        Vector4 objectColour;
    };

    // ── 预上传 instanced batch SSBO（cascade 间矩阵不变，只需上传一次）──
    for (const auto& batch : m_instancedBatches) {
        if ((int)batch.objects.size() < INSTANCE_THRESHOLD) continue;
        if (batch.shadowCascadeMask == 0) continue;

        int count = (int)batch.objects.size();
        size_t requiredSize = sizeof(ShadowInstanceData) * count;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO);
        if (requiredSize > m_instanceSSBOSize) {
            glBufferData(GL_SHADER_STORAGE_BUFFER, requiredSize, nullptr, GL_DYNAMIC_DRAW);
            m_instanceSSBOSize = requiredSize;
        }

        ShadowInstanceData* ptr = (ShadowInstanceData*)glMapBufferRange(
            GL_SHADER_STORAGE_BUFFER, 0, requiredSize,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        if (ptr) {
            for (int i = 0; i < count; ++i) {
                ptr[i].modelMatrix  = batch.objects[i]->GetTransform().GetMatrix();
                ptr[i].objectColour = Vector4(1, 1, 1, 1);
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }

    for (int c = 0; c < NUM_CASCADES; c++) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO[c]);
        glViewport(0, 0, m_shadowRes[c], m_shadowRes[c]);
        glClear(GL_DEPTH_BUFFER_BIT);

        Matrix4 lvp = m_lightProjMat[c] * m_lightViewMat[c];

        // ── Instanced shadow draw（SSBO 已在循环外上传）──
        for (const auto& batch : m_instancedBatches) {
            if ((int)batch.objects.size() < INSTANCE_THRESHOLD) continue;
            if (!(batch.shadowCascadeMask & (1 << c))) continue;

            UseShader(*shadowShader);
            GLuint pid = shadowShader->GetProgramID();
            glUniform1i(glGetUniformLocation(pid, "useModelMatrix"), 0);
            glUniform1i(glGetUniformLocation(pid, "useInstancing"), 1);
            glUniformMatrix4fv(glGetUniformLocation(pid, "lightVPMatrix"), 1, false, (float*)&lvp);
            glUniform1f(glGetUniformLocation(pid, "normalOffsetBias"), m_shadowNormalOffset[c]);
            glUniform1i(glGetUniformLocation(pid, "useSkinning"), 0);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_instanceSSBO);

            int count = (int)batch.objects.size();
            BindMesh(*batch.mesh);
            size_t layerCount = batch.mesh->GetSubMeshCount();
            if (layerCount == 0) DrawBoundMesh(0, count);
            else for (size_t j = 0; j < layerCount; j++) DrawBoundMesh((uint32_t)j, count);

            glUniform1i(glGetUniformLocation(pid, "useInstancing"), 0);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        }

        // ── Ocean pillar shadow draw（仅 cascade 2）──
        if ((0x04 & (1 << c)) && m_oceanSSBOReady) {
            DrawOceanShadowBatch(shadowShader, lvp, m_shadowNormalOffset[c]);
        }

        // ── Per-object shadow draw (small batches + non-instanced) ──
        for (const auto& i : list) {
            const RenderObject* o = i.object;

            if (o->instanced) continue;

            Matrix4 model = o->GetTransform().GetMatrix();

            if (o->GetMaterial().alphaMode == AlphaMode::Mask) {
                UseShader(*m_shadowAlphaTestShader);
                GLint curProg = m_shadowAlphaTestShader->GetProgramID();
                glUniform1i(glGetUniformLocation(curProg, "useModelMatrix"), 1);
                glUniform1i(glGetUniformLocation(curProg, "useInstancing"), 0);
                glUniformMatrix4fv(glGetUniformLocation(curProg, "lightVPMatrix"), 1, false, (float*)&lvp);
                glUniform1f(glGetUniformLocation(curProg, "normalOffsetBias"), m_shadowNormalOffset[c]);
                glUniformMatrix4fv(glGetUniformLocation(curProg, "modelMatrix"), 1, false, (float*)&model);
                glUniform1f(glGetUniformLocation(curProg, "alphaCutoff"), o->GetMaterial().alphaCutoff);
                if (o->GetMaterial().diffuseTex) {
                    OGLTexture* t = (OGLTexture*)o->GetMaterial().diffuseTex;
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, t->GetObjectID());
                    glUniform1i(glGetUniformLocation(curProg, "albedoTex"), 0);
                }
            } else {
                UseShader(*shadowShader);
                GLint curProg = shadowShader->GetProgramID();
                glUniform1i(glGetUniformLocation(curProg, "useModelMatrix"), 1);
                glUniform1i(glGetUniformLocation(curProg, "useInstancing"), 0);
                glUniformMatrix4fv(glGetUniformLocation(curProg, "lightVPMatrix"), 1, false, (float*)&lvp);
                glUniform1f(glGetUniformLocation(curProg, "normalOffsetBias"), m_shadowNormalOffset[c]);
                glUniformMatrix4fv(glGetUniformLocation(curProg, "modelMatrix"), 1, false, (float*)&model);
            }

            GLint curProg = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &curProg);
            glUniform1i(glGetUniformLocation(curProg, "useSkinning"), o->useSkinning ? 1 : 0);
            if (o->useSkinning && !o->skinBoneMatrices.empty()) {
                glUniformMatrix4fv(glGetUniformLocation(curProg, "boneMatrices"),
                                   (int)o->skinBoneMatrices.size(), false,
                                   (float*)o->skinBoneMatrices.data());
            }

            BindMesh((OGLMesh&)*o->GetMesh());
            size_t layerCount = o->GetMesh()->GetSubMeshCount();
            if (layerCount == 0) DrawBoundMesh(0);
            else for (size_t j = 0; j < layerCount; j++) DrawBoundMesh((uint32_t)j);
        }
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei)windowSize.x, (GLsizei)windowSize.y);
}

// ============================================================
// RenderSkyboxPass
// ============================================================

void GameTechRenderer::RenderSkyboxPass() {
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    Matrix4 viewMatrix = gameWorld.GetMainCamera().BuildViewMatrix();
    Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());

    UseShader(*skyboxShader);
    int projLoc = glGetUniformLocation(skyboxShader->GetProgramID(), "projMatrix");
    int viewLoc = glGetUniformLocation(skyboxShader->GetProgramID(), "viewMatrix");
    int texLoc  = glGetUniformLocation(skyboxShader->GetProgramID(), "cubeTex");
    glUniformMatrix4fv(projLoc, 1, false, (float*)&projMatrix);
    glUniformMatrix4fv(viewLoc, 1, false, (float*)&viewMatrix);
    glUniform1i(texLoc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);

    BindMesh(*skyboxMesh);
    DrawBoundMesh();

    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================
// BindCommonSceneUniforms — 上传视图/光照/阴影 uniform（两套 shader 共用）
// ============================================================

static void BindCommonSceneUniforms(
    OGLShader* shader,
    const Matrix4& viewMatrix, const Matrix4& projMatrix,
    const Vector3& camPos, const Vector3& sunPos, const Vector3& sunCol,
    GLuint shadowTex0, GLuint shadowTex1, GLuint shadowTex2,
    const Matrix4& shadowMat0, const Matrix4& shadowMat1, const Matrix4& shadowMat2,
    const float cascadeSplits[3], float pcssLightSize,
    GLuint irradianceMap, GLuint prefilterMap, GLuint brdfLUT, float iblIntensity,
    float shadowBiasSlope, float shadowBiasConstant)
{
    GLuint pid = shader->GetProgramID();
    auto ul = [&](const char* n) { return glGetUniformLocation(pid, n); };

    glUniformMatrix4fv(ul("viewMatrix"), 1, false, (float*)&viewMatrix);
    glUniformMatrix4fv(ul("projMatrix"), 1, false, (float*)&projMatrix);
    glUniform3fv(ul("cameraPos"),    1, (float*)&camPos);
    glUniform3fv(ul("sunPos"),       1, (float*)&sunPos);
    glUniform3fv(ul("sunColour"),    1, (float*)&sunCol);
    glUniform1f(ul("sunRadius"),     10000.0f);

    // CSM 阴影纹理 (units 5, 6, 7)
    glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, shadowTex0);
    glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, shadowTex1);
    glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, shadowTex2);
    glUniform1i(ul("shadowTex0"), 5);
    glUniform1i(ul("shadowTex1"), 6);
    glUniform1i(ul("shadowTex2"), 7);
    glUniformMatrix4fv(ul("shadowMatrix0"), 1, false, (float*)&shadowMat0);
    glUniformMatrix4fv(ul("shadowMatrix1"), 1, false, (float*)&shadowMat1);
    glUniformMatrix4fv(ul("shadowMatrix2"), 1, false, (float*)&shadowMat2);
    glUniform1fv(ul("cascadeSplits"), 3, cascadeSplits);
    glUniform1f(ul("pcssLightSize"),  pcssLightSize);
    glUniform1f(ul("shadowBiasSlope"),    shadowBiasSlope);
    glUniform1f(ul("shadowBiasConstant"), shadowBiasConstant);

    // IBL (units 8, 9, 10)
    glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, brdfLUT);
    glUniform1i(ul("irradianceMap"), 8);
    glUniform1i(ul("prefilterMap"),  9);
    glUniform1i(ul("brdfLUT"),       10);
    glUniform1f(ul("iblIntensity"),  iblIntensity);
    glUniform1i(ul("useIBL"), (irradianceMap != 0 && prefilterMap != 0 && brdfLUT != 0) ? 1 : 0);
}

// ============================================================
// DrawObject — 绑定材质纹理并绘制单个物体
// ============================================================

/**
 * @brief 绑定单个 RenderObject 的材质参数、纹理、骨骼蒙皮数据后提交绘制。
 *
 * @details
 * 纹理绑定使用语义正确的 fallback 纹理（Phase 6B）：
 *   - slot 0 albedo   → m_fallbackWhiteTex  (255,255,255)
 *   - slot 1 normal   → m_fallbackNormalTex  (128,128,255) 中性法线
 *   - slot 2 ORM      → m_fallbackOrmTex     (255,128,0)   AO=1/rough=0.5/metal=0
 *   - slot 3 emissive → m_fallbackBlackTex   (0,0,0)       无自发光
 *
 * doubleSided 材质会临时 glDisable(GL_CULL_FACE)，绘制完毕后恢复。
 */
void GameTechRenderer::DrawObject(OGLShader* shader,
                                  const RenderObject* o)
{
    GLuint pid = shader->GetProgramID();
    auto ul = [&](const char* n) { return glGetUniformLocation(pid, n); };

    Matrix4 modelMatrix = o->GetTransform().GetMatrix();
    glUniformMatrix4fv(ul("modelMatrix"), 1, false, (float*)&modelMatrix);

    const GameTechMaterial& mat = o->GetMaterial();
    Vector4 colour = o->GetColour();
    glUniform4fv(ul("objectColour"), 1, &colour.x);

    // 纹理绑定 (units 0-3)
    auto bindTex = [&](Texture* tex, const char* samplerName, int unit, GLuint fallbackID) {
        glActiveTexture(GL_TEXTURE0 + unit);
        if (tex) {
            OGLTexture* t = (OGLTexture*)tex;
            glBindTexture(GL_TEXTURE_2D, t->GetObjectID());
        } else {
            glBindTexture(GL_TEXTURE_2D, fallbackID);
        }
        glUniform1i(ul(samplerName), unit);
    };

    bindTex(mat.diffuseTex,  "albedoTex",   0, m_fallbackWhiteTex);
    bindTex(mat.bumpTex,     "normalTex",   1, m_fallbackNormalTex);
    bindTex(mat.ormTex,      "ormTex",      2, m_fallbackOrmTex);
    bindTex(mat.emissiveTex, "emissiveTex", 3, m_fallbackBlackTex);

    glUniform1i(ul("hasTexture"),    mat.diffuseTex  ? 1 : 0);
    glUniform1i(ul("hasBumpTex"),    mat.bumpTex     ? 1 : 0);
    glUniform1i(ul("hasOrmTex"),     mat.ormTex      ? 1 : 0);
    glUniform1i(ul("hasEmissiveTex"),mat.emissiveTex ? 1 : 0);
    glUniform1i(ul("hasVertexColours"), !o->GetMesh()->GetColourData().empty() ? 1 : 0);

    // 材质参数
    glUniform1f(ul("metallic"),          mat.metallic);
    glUniform1f(ul("roughness"),         mat.roughness);
    glUniform1f(ul("ao"),                mat.ao);
    glUniform3fv(ul("emissiveColor"),    1, (float*)&mat.emissiveColor);
    glUniform1f(ul("emissiveStrength"),  mat.emissiveStrength);
    glUniform3fv(ul("rimColour"),        1, (float*)&mat.rimColour);
    glUniform1f(ul("rimPower"),          mat.rimPower);
    glUniform1f(ul("rimStrength"),       mat.rimStrength);

    // Alpha 模式
    glUniform1i(ul("alphaMode"),    (int)mat.alphaMode);
    glUniform1f(ul("alphaCutoff"),  mat.alphaCutoff);

    // 双面渲染
    if (mat.doubleSided) glDisable(GL_CULL_FACE);
    glUniform1i(ul("doubleSided"), mat.doubleSided ? 1 : 0);

    // 骨骼蒙皮
    glUniform1i(ul("useSkinning"), o->useSkinning ? 1 : 0);
    if (o->useSkinning && !o->skinBoneMatrices.empty()) {
        int count = (int)o->skinBoneMatrices.size();
        glUniformMatrix4fv(ul("boneMatrices"), count, false,
                           (float*)o->skinBoneMatrices.data());
    }

    OGLMesh* mesh = (OGLMesh*)o->GetMesh();
    BindMesh(*mesh);
    size_t layerCount = mesh->GetSubMeshCount();
    if (layerCount == 0) DrawBoundMesh(0);
    else for (size_t i = 0; i < layerCount; i++) DrawBoundMesh((uint32_t)i);

    if (mat.doubleSided) glEnable(GL_CULL_FACE);
}

// ============================================================
// DrawInstancedBatch — SSBO 上传 + instanced draw
// ============================================================

/**
 * @brief 将同 mesh 批次的 modelMatrix[] 和 objectColour[] 上传到 SSBO，
 *        执行 glDrawElementsInstanced 一次绘制全部实例。
 *
 * @details
 * SSBO 布局（binding point 0）：
 *   struct InstanceData {
 *       mat4 modelMatrix;   // 64 bytes
 *       vec4 objectColour;  // 16 bytes
 *   };                      // 80 bytes per instance, std430
 *
 * Shader 通过 gl_InstanceID 索引 SSBO 获取每实例数据。
 * 使用 GL_DYNAMIC_DRAW + orphaning（glBufferData 重新分配）策略，
 * 避免 GPU/CPU 同步等待。
 *
 * PS5 移植注意：
 *   - SSBO 对应 AGC 的 RW_Buffer / StructuredBuffer
 *   - glBufferData orphaning 对应 AGC 的 ring buffer 分配
 *   - binding point 0 需映射到 PS5 的 buffer slot
 *   - 回退方案：删除 SSBO 路径，走原有 DrawObject 逐个绘制
 */
void GameTechRenderer::DrawInstancedBatch(OGLShader* shader, const InstancedBatch& batch,
                                          const Matrix4& viewMatrix, const Matrix4& projMatrix)
{
    int count = (int)batch.objects.size();
    if (count == 0) return;

    GLuint pid = shader->GetProgramID();
    auto ul = [&](const char* n) { return glGetUniformLocation(pid, n); };

    glUniformMatrix4fv(ul("viewMatrix"), 1, false, (float*)&viewMatrix);
    glUniformMatrix4fv(ul("projMatrix"), 1, false, (float*)&projMatrix);
    glUniform1i(ul("useSkinning"), 0);
    glUniform1i(ul("useInstancing"), 1);

    struct InstanceData {
        Matrix4 modelMatrix;
        Vector4 objectColour;
    };

    size_t requiredSize = sizeof(InstanceData) * count;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_instanceSSBO);
    if (requiredSize > m_instanceSSBOSize) {
        glBufferData(GL_SHADER_STORAGE_BUFFER, requiredSize, nullptr, GL_DYNAMIC_DRAW);
        m_instanceSSBOSize = requiredSize;
    }

    InstanceData* ptr = (InstanceData*)glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0, requiredSize,
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr) {
        for (int i = 0; i < count; ++i) {
            ptr[i].modelMatrix  = batch.objects[i]->GetTransform().GetMatrix();
            ptr[i].objectColour = batch.objects[i]->GetColour();
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_instanceSSBO);

    const RenderObject* first = batch.objects[0];
    const GameTechMaterial& mat = first->GetMaterial();

    auto bindTex = [&](Texture* tex, const char* samplerName, int unit, GLuint fallbackID) {
        glActiveTexture(GL_TEXTURE0 + unit);
        if (tex) {
            OGLTexture* t = (OGLTexture*)tex;
            glBindTexture(GL_TEXTURE_2D, t->GetObjectID());
        } else {
            glBindTexture(GL_TEXTURE_2D, fallbackID);
        }
        glUniform1i(ul(samplerName), unit);
    };

    bindTex(mat.diffuseTex,  "albedoTex",   0, m_fallbackWhiteTex);
    bindTex(mat.bumpTex,     "normalTex",   1, m_fallbackNormalTex);
    bindTex(mat.ormTex,      "ormTex",      2, m_fallbackOrmTex);
    bindTex(mat.emissiveTex, "emissiveTex", 3, m_fallbackBlackTex);

    glUniform1i(ul("hasTexture"),    mat.diffuseTex  ? 1 : 0);
    glUniform1i(ul("hasBumpTex"),    mat.bumpTex     ? 1 : 0);
    glUniform1i(ul("hasOrmTex"),     mat.ormTex      ? 1 : 0);
    glUniform1i(ul("hasEmissiveTex"),mat.emissiveTex ? 1 : 0);
    glUniform1i(ul("hasVertexColours"), 0);

    glUniform1f(ul("metallic"),          mat.metallic);
    glUniform1f(ul("roughness"),         mat.roughness);
    glUniform1f(ul("ao"),                mat.ao);
    glUniform3fv(ul("emissiveColor"),    1, (float*)&mat.emissiveColor);
    glUniform1f(ul("emissiveStrength"),  mat.emissiveStrength);
    glUniform3fv(ul("rimColour"),        1, (float*)&mat.rimColour);
    glUniform1f(ul("rimPower"),          mat.rimPower);
    glUniform1f(ul("rimStrength"),       mat.rimStrength);
    glUniform1i(ul("alphaMode"),    0);
    glUniform1f(ul("alphaCutoff"),  0.5f);
    glUniform1i(ul("doubleSided"),  0);

    BindMesh(*batch.mesh);
    size_t layerCount = batch.mesh->GetSubMeshCount();
    if (layerCount == 0) DrawBoundMesh(0, count);
    else for (size_t i = 0; i < layerCount; i++) DrawBoundMesh((uint32_t)i, count);

    glUniform1i(ul("useInstancing"), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
}

// ============================================================
// RenderOpaquePass
// ============================================================

void GameTechRenderer::RenderOpaquePass(std::vector<ObjectSortState>& list) {
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_CULL_FACE);
    }

    Matrix4 viewMatrix = gameWorld.GetMainCamera().BuildViewMatrix();
    Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());
    Vector3 camPos     = gameWorld.GetMainCamera().GetPosition();
    Vector3 sunPos     = gameWorld.GetSunPosition();
    Vector3 sunCol     = gameWorld.GetSunColour();

    // ── Instanced batches（>= INSTANCE_THRESHOLD 的同 mesh 批次）──
    for (const auto& batch : m_instancedBatches) {
        if ((int)batch.objects.size() >= INSTANCE_THRESHOLD) {
            UseShader(*batch.shader);
            BindCommonSceneUniforms(batch.shader, viewMatrix, projMatrix, camPos, sunPos, sunCol,
                                    m_shadowTex[0], m_shadowTex[1], m_shadowTex[2],
                                    m_shadowMatrix[0], m_shadowMatrix[1], m_shadowMatrix[2],
                                    m_cascadeSplits, m_pcssLightSize,
                                    m_irradianceMap, m_prefilterMap, m_brdfLUT, m_iblIntensity,
                                    m_shadowBiasSlope, m_shadowBiasConstant);
            DrawInstancedBatch(batch.shader, batch, viewMatrix, projMatrix);
        }
    }

    // ── Ocean pillar batch（GPU 噪波驱动）──
    if (m_oceanSSBOReady && m_oceanPillarCount > 0) {
        UseShader(*defaultShader);
        BindCommonSceneUniforms(defaultShader, viewMatrix, projMatrix, camPos, sunPos, sunCol,
                                m_shadowTex[0], m_shadowTex[1], m_shadowTex[2],
                                m_shadowMatrix[0], m_shadowMatrix[1], m_shadowMatrix[2],
                                m_cascadeSplits, m_pcssLightSize,
                                m_irradianceMap, m_prefilterMap, m_brdfLUT, m_iblIntensity,
                                m_shadowBiasSlope, m_shadowBiasConstant);
        DrawOceanBatch(defaultShader, viewMatrix, projMatrix);
    }

    // ── 逐个绘制（小批次 + 非 instanced 对象）──
    OGLShader* lastShader = nullptr;

    for (const auto& state : list) {
        const RenderObject* o = state.object;
        const GameTechMaterial& mat = o->GetMaterial();

        if (mat.alphaMode == AlphaMode::Mask ||
            mat.type       == MaterialType::Transparent) continue;

        if (o->instanced) continue;

        OGLShader* shader = (mat.shadingModel == ShadingModel::PBR) ? m_pbrShader : defaultShader;
        if (shader != lastShader) {
            UseShader(*shader);
            BindCommonSceneUniforms(shader, viewMatrix, projMatrix, camPos, sunPos, sunCol,
                                    m_shadowTex[0], m_shadowTex[1], m_shadowTex[2],
                                    m_shadowMatrix[0], m_shadowMatrix[1], m_shadowMatrix[2],
                                    m_cascadeSplits, m_pcssLightSize,
                                    m_irradianceMap, m_prefilterMap, m_brdfLUT, m_iblIntensity,
                                    m_shadowBiasSlope, m_shadowBiasConstant);
            lastShader = shader;
        }

        DrawObject(shader, o);
    }

    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }
}

// ============================================================
// RenderAlphaMaskPass — discard in shader，写深度
// ============================================================

void GameTechRenderer::RenderAlphaMaskPass(std::vector<ObjectSortState>& list) {
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    Matrix4 viewMatrix = gameWorld.GetMainCamera().BuildViewMatrix();
    Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());
    Vector3 camPos     = gameWorld.GetMainCamera().GetPosition();
    Vector3 sunPos     = gameWorld.GetSunPosition();
    Vector3 sunCol     = gameWorld.GetSunColour();

    OGLShader* lastShader = nullptr;

    for (const auto& state : list) {
        const RenderObject* o = state.object;
        const GameTechMaterial& mat = o->GetMaterial();
        if (mat.alphaMode != AlphaMode::Mask) continue;

        OGLShader* shader = (mat.shadingModel == ShadingModel::PBR) ? m_pbrShader : defaultShader;
        if (shader != lastShader) {
            UseShader(*shader);
            BindCommonSceneUniforms(shader, viewMatrix, projMatrix, camPos, sunPos, sunCol,
                                    m_shadowTex[0], m_shadowTex[1], m_shadowTex[2],
                                    m_shadowMatrix[0], m_shadowMatrix[1], m_shadowMatrix[2],
                                    m_cascadeSplits, m_pcssLightSize,
                                    m_irradianceMap, m_prefilterMap, m_brdfLUT, m_iblIntensity,
                                    m_shadowBiasSlope, m_shadowBiasConstant);
            lastShader = shader;
        }

        DrawObject(shader, o);
    }
}

// ============================================================
// RenderTransparentPass — 半透明（back-to-front）
// ============================================================

/**
 * @brief 半透明物体渲染 pass，按距离从远到近排序后逐物体绘制。
 *
 * @details
 * 对单面透明物体采用经典 OIT-lite 两 pass 策略：先 cull front（画背面），再 cull back（画正面），
 * 保证内层表面先写入 framebuffer，外层表面在其上混合。
 * 对双面透明物体只调用一次 DrawObject — DrawObject 内部已 glDisable(GL_CULL_FACE)，
 * 避免 4× 几何开销（Phase 6A 修复）。
 */
void GameTechRenderer::RenderTransparentPass(std::vector<ObjectSortState>& list) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_CULL_FACE);
    }

    Matrix4 viewMatrix = gameWorld.GetMainCamera().BuildViewMatrix();
    Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());
    Vector3 camPos     = gameWorld.GetMainCamera().GetPosition();
    Vector3 sunPos     = gameWorld.GetSunPosition();
    Vector3 sunCol     = gameWorld.GetSunColour();

    OGLShader* lastShader = nullptr;

    for (const auto& state : list) {
        const RenderObject* o = state.object;
        const GameTechMaterial& mat = o->GetMaterial();

        OGLShader* shader = (mat.shadingModel == ShadingModel::PBR) ? m_pbrShader : defaultShader;
        if (shader != lastShader) {
            UseShader(*shader);
            BindCommonSceneUniforms(shader, viewMatrix, projMatrix, camPos, sunPos, sunCol,
                                    m_shadowTex[0], m_shadowTex[1], m_shadowTex[2],
                                    m_shadowMatrix[0], m_shadowMatrix[1], m_shadowMatrix[2],
                                    m_cascadeSplits, m_pcssLightSize,
                                    m_irradianceMap, m_prefilterMap, m_brdfLUT, m_iblIntensity,
                                    m_shadowBiasSlope, m_shadowBiasConstant);
            lastShader = shader;
        }

        if (!m_wireframeMode) {
            if (mat.doubleSided) {
                // 双面：DrawObject 内部已 disable cull，一次绘制即渲染两面
                DrawObject(shader, o);
            } else {
                // 单面：先背面后正面，经典 OIT-lite 两 pass
                glCullFace(GL_FRONT);
                DrawObject(shader, o);
                glCullFace(GL_BACK);
                DrawObject(shader, o);
            }
        } else {
            DrawObject(shader, o);
        }
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    if (m_wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }
}

// ============================================================
// RenderPostProcessChain
// ============================================================

void GameTechRenderer::RenderPostProcessChain() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    const int w = (int)windowSize.x;
    const int h = (int)windowSize.y;
    float invW = 1.0f / w, invH = 1.0f / h;

    // ── SSAO ──────────────────────────────────────────
    if (m_ssaoEnabled && m_ssaoShader) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
        glViewport(0, 0, w, h);
        UseShader(*m_ssaoShader);
        GLuint pid = m_ssaoShader->GetProgramID();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_hdrNormalTex);
        glUniform1i(glGetUniformLocation(pid, "normalTex"), 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
        glUniform1i(glGetUniformLocation(pid, "depthTex"), 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_ssaoNoiseTex);
        glUniform1i(glGetUniformLocation(pid, "noiseTex"), 2);
        for (int i = 0; i < 64; i++) {
            char buf[32]; snprintf(buf, sizeof(buf), "ssaoKernel[%d]", i);
            glUniform3fv(glGetUniformLocation(pid, buf), 1, (float*)&m_ssaoKernel[i]);
        }
        Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());
        Matrix4 invProj    = Maths::Matrix::Inverse(projMatrix);
        glUniformMatrix4fv(glGetUniformLocation(pid, "projMatrix"),    1, false, (float*)&projMatrix);
        glUniformMatrix4fv(glGetUniformLocation(pid, "invProjMatrix"), 1, false, (float*)&invProj);
        glUniform1f(glGetUniformLocation(pid, "ssaoRadius"), m_ssaoRadius);
        glUniform1f(glGetUniformLocation(pid, "ssaoBias"),   m_ssaoBias);
        glUniform2f(glGetUniformLocation(pid, "screenSize"), (float)w, (float)h);
        DrawFullscreenTriangle();

        // SSAO Blur
        glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
        UseShader(*m_ssaoBlurShader);
        pid = m_ssaoBlurShader->GetProgramID();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_ssaoTex);
        glUniform1i(glGetUniformLocation(pid, "ssaoRawTex"), 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_hdrDepthTex);
        glUniform1i(glGetUniformLocation(pid, "depthTex"), 1);
        glUniform2f(glGetUniformLocation(pid, "texelSize"), invW, invH);
        DrawFullscreenTriangle();
    }

    // ── Bloom Extract → ppTex[0] ─────────────────────
    if (m_bloomEnabled && m_bloomExtractShader) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_ppFBO[0]);
        glViewport(0, 0, w, h);
        UseShader(*m_bloomExtractShader);
        GLuint pid = m_bloomExtractShader->GetProgramID();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
        glUniform1i(glGetUniformLocation(pid, "hdrColorTex"), 0);
        glUniform1f(glGetUniformLocation(pid, "bloomThreshold"), m_bloomThreshold);
        DrawFullscreenTriangle();

        // Kawase Blur × 5 ping-pong
        if (m_bloomBlurShader) {
            for (int iter = 0; iter < 5; iter++) {
                int src = iter % 2, dst = 1 - src;
                glBindFramebuffer(GL_FRAMEBUFFER, m_ppFBO[dst]);
                UseShader(*m_bloomBlurShader);
                GLuint bpid = m_bloomBlurShader->GetProgramID();
                glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_ppTex[src]);
                glUniform1i(glGetUniformLocation(bpid, "bloomTex"), 0);
                glUniform1i(glGetUniformLocation(bpid, "iteration"), iter);
                glUniform2f(glGetUniformLocation(bpid, "texelSize"), invW, invH);
                DrawFullscreenTriangle();
            }
        }
    }

    // ── Composite + Tonemap → ppTex[0] ──────────────
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_ppFBO[0]);
        glViewport(0, 0, w, h);
        UseShader(*m_tonemapShader);
        GLuint pid = m_tonemapShader->GetProgramID();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_hdrColorTex);
        glUniform1i(glGetUniformLocation(pid, "hdrTex"), 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_ssaoBlurTex);
        glUniform1i(glGetUniformLocation(pid, "ssaoTex"), 1);
        // bloom result is in ppTex[1] after 5 odd iterations (iter=4 → dst=1)
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_ppTex[1]);
        glUniform1i(glGetUniformLocation(pid, "bloomTex"), 2);
        glUniform1f(glGetUniformLocation(pid, "exposure"),         m_exposure);
        glUniform1f(glGetUniformLocation(pid, "bloomStrength"),    m_bloomStrength);
        glUniform1f(glGetUniformLocation(pid, "vignetteStrength"), m_vignetteStrength);
        glUniform1i(glGetUniformLocation(pid, "ssaoEnabled"),      m_ssaoEnabled ? 1 : 0);
        glUniform1i(glGetUniformLocation(pid, "bloomEnabled"),     m_bloomEnabled ? 1 : 0);
        DrawFullscreenTriangle();
    }

    // ── FXAA → default FBO ───────────────────────────
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        UseShader(*m_fxaaShader);
        GLuint pid = m_fxaaShader->GetProgramID();
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_ppTex[0]);
        glUniform1i(glGetUniformLocation(pid, "screenTex"), 0);
        glUniform2f(glGetUniformLocation(pid, "texelSize"), invW, invH);
        DrawFullscreenTriangle();
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// ============================================================
// Debug Overlay
// ============================================================

void GameTechRenderer::RenderLines() {
    const std::vector<Debug::DebugLineEntry>& lines = Debug::GetDebugLines();
    if (lines.empty()) return;

    Matrix4 viewMatrix = gameWorld.GetMainCamera().BuildViewMatrix();
    Matrix4 projMatrix = gameWorld.GetMainCamera().BuildProjectionMatrix(hostWindow.GetScreenAspect());
    Matrix4 viewProj   = projMatrix * viewMatrix;

    UseShader(*debugShader);
    int matSlot = glGetUniformLocation(debugShader->GetProgramID(), "viewProjMatrix");
    GLuint texSlot = glGetUniformLocation(debugShader->GetProgramID(), "useTexture");
    glUniform1i(texSlot, 0);
    glUniformMatrix4fv(matSlot, 1, false, (float*)viewProj.array);

    debugLineData.clear();
    size_t frameLineCount = lines.size() * 2;
    SetDebugLineBufferSizes(frameLineCount);

    glBindBuffer(GL_ARRAY_BUFFER, lineVertVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lines.size() * sizeof(Debug::DebugLineEntry), lines.data());

    glBindVertexArray(lineVAO);
    glDrawArrays(GL_LINES, 0, (GLsizei)frameLineCount);
    glBindVertexArray(0);
}

void GameTechRenderer::RenderText() {
    const std::vector<Debug::DebugStringEntry>& strings = Debug::GetDebugStrings();
    if (strings.empty()) return;

    UseShader(*debugShader);

    OGLTexture* t = (OGLTexture*)Debug::GetDebugFont()->GetTexture();
    if (t) {
        BindTextureToShader(*t, "mainTex", 0);
    }

    Matrix4 proj = Matrix::Orthographic(0.0f, 100.0f, 100.0f, 0.0f, -1.0f, 1.0f);
    int matSlot  = glGetUniformLocation(debugShader->GetProgramID(), "viewProjMatrix");
    glUniformMatrix4fv(matSlot, 1, false, (float*)proj.array);
    GLuint texSlot = glGetUniformLocation(debugShader->GetProgramID(), "useTexture");
    glUniform1i(texSlot, 1);

    debugTextPos.clear();
    debugTextColours.clear();
    debugTextUVs.clear();

    int frameVertCount = 0;
    for (const auto& s : strings)
        frameVertCount += Debug::GetDebugFont()->GetVertexCountForString(s.data);
    SetDebugStringBufferSizes(frameVertCount);

    for (const auto& s : strings)
        Debug::GetDebugFont()->BuildVerticesForString(s.data, s.position, s.colour, 20.0f,
                                                      debugTextPos, debugTextUVs, debugTextColours);

    glBindBuffer(GL_ARRAY_BUFFER, textVertVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, frameVertCount * sizeof(Vector3), debugTextPos.data());
    glBindBuffer(GL_ARRAY_BUFFER, textColourVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, frameVertCount * sizeof(Vector4), debugTextColours.data());
    glBindBuffer(GL_ARRAY_BUFFER, textTexVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, frameVertCount * sizeof(Vector2), debugTextUVs.data());

    glBindVertexArray(textVAO);
    glDrawArrays(GL_TRIANGLES, 0, frameVertCount);
    glBindVertexArray(0);
}

void GameTechRenderer::RenderTextures() {
    const std::vector<Debug::DebugTexEntry>& texEntries = Debug::GetDebugTex();
    if (texEntries.empty()) return;

    UseShader(*debugShader);

    Matrix4 proj = Matrix::Orthographic(0.0f, 100.0f, 100.0f, 0.0f, -1.0f, 1.0f);
    int matSlot = glGetUniformLocation(debugShader->GetProgramID(), "viewProjMatrix");
    glUniformMatrix4fv(matSlot, 1, false, (float*)proj.array);

    GLuint texSlot   = glGetUniformLocation(debugShader->GetProgramID(), "useTexture");
    GLuint useColour = glGetUniformLocation(debugShader->GetProgramID(), "useColour");
    GLuint colSlot   = glGetUniformLocation(debugShader->GetProgramID(), "texColour");
    glUniform1i(texSlot,   2);
    glUniform1i(useColour, 1);

    BindMesh(*debugTexMesh);
    glActiveTexture(GL_TEXTURE0);

    for (const auto& tex : texEntries) {
        OGLTexture* t = (OGLTexture*)tex.t;
        glBindTexture(GL_TEXTURE_2D, t->GetObjectID());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        BindTextureToShader(*t, "mainTex", 0);

        Matrix4 transform = Matrix::Translation(Vector3(tex.position.x, tex.position.y, 0))
                          * Matrix::Scale(Vector3(tex.scale.x, tex.scale.y, 1.0f));
        Matrix4 finalMatrix = proj * transform;
        glUniformMatrix4fv(matSlot, 1, false, (float*)finalMatrix.array);
        glUniform4f(colSlot, tex.colour.x, tex.colour.y, tex.colour.z, tex.colour.w);
        DrawBoundMesh();
    }

    glUniform1i(useColour, 0);
}

// ============================================================
// Buffer resize helpers
// ============================================================

void GameTechRenderer::SetDebugStringBufferSizes(size_t newVertCount) {
    if (newVertCount <= textCount) return;
    textCount = newVertCount;

    glBindBuffer(GL_ARRAY_BUFFER, textVertVBO);
    glBufferData(GL_ARRAY_BUFFER, textCount * sizeof(Vector3), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, textColourVBO);
    glBufferData(GL_ARRAY_BUFFER, textCount * sizeof(Vector4), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, textTexVBO);
    glBufferData(GL_ARRAY_BUFFER, textCount * sizeof(Vector2), nullptr, GL_DYNAMIC_DRAW);

    debugTextPos.reserve(textCount);
    debugTextColours.reserve(textCount);
    debugTextUVs.reserve(textCount);

    glBindVertexArray(textVAO);
    glVertexAttribFormat(0, 3, GL_FLOAT, false, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexBuffer(0, textVertVBO, 0, sizeof(Vector3));
    glVertexAttribFormat(1, 4, GL_FLOAT, false, 0);
    glVertexAttribBinding(1, 1);
    glBindVertexBuffer(1, textColourVBO, 0, sizeof(Vector4));
    glVertexAttribFormat(2, 2, GL_FLOAT, false, 0);
    glVertexAttribBinding(2, 2);
    glBindVertexBuffer(2, textTexVBO, 0, sizeof(Vector2));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void GameTechRenderer::SetDebugLineBufferSizes(size_t newVertCount) {
    if (newVertCount <= lineCount) return;
    lineCount = newVertCount;

    glBindBuffer(GL_ARRAY_BUFFER, lineVertVBO);
    glBufferData(GL_ARRAY_BUFFER, lineCount * sizeof(Debug::DebugLineEntry), nullptr, GL_DYNAMIC_DRAW);
    debugLineData.reserve(lineCount);

    glBindVertexArray(lineVAO);
    int realStride = sizeof(Debug::DebugLineEntry) / 2;
    glVertexAttribFormat(0, 3, GL_FLOAT, false, offsetof(Debug::DebugLineEntry, start));
    glVertexAttribBinding(0, 0);
    glBindVertexBuffer(0, lineVertVBO, 0, realStride);
    glVertexAttribFormat(1, 4, GL_FLOAT, false, offsetof(Debug::DebugLineEntry, colourA));
    glVertexAttribBinding(1, 0);
    glBindVertexBuffer(1, lineVertVBO, sizeof(Vector4), realStride);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
