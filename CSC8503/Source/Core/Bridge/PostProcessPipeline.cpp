#include "PostProcessPipeline.h"
#include "OGLShader.h"
#include <iostream>

using namespace NCL::Rendering;

namespace ECS {

void PostProcessPipeline::Init(int width, int height) {
    if (m_Initialized) return;

    // Load shaders
    bloomExtractShader = new OGLShader("fullscreen_quad.vert", "bloom_extract.frag");
    bloomBlurShader    = new OGLShader("fullscreen_quad.vert", "bloom_blur.frag");
    tonemapShader      = new OGLShader("fullscreen_quad.vert", "tonemap_composite.frag");

    // Empty VAO for fullscreen triangle
    glGenVertexArrays(1, &emptyVAO);

    CreateBloomFBOs(width, height);
    m_Initialized = true;
}

void PostProcessPipeline::Resize(int width, int height) {
    if (width == m_Width && height == m_Height) return;
    DestroyBloomFBOs();
    CreateBloomFBOs(width, height);
}

void PostProcessPipeline::Execute(GLuint hdrColorTex, const PostProcessParams& params) {
    if (!m_Initialized) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // ── 1. Bloom Extract ────────────────────────────────────────────
    if (params.enableBloom) {
        glBindFramebuffer(GL_FRAMEBUFFER, extractFBO);
        glViewport(0, 0, m_Width / 2, m_Height / 2);
        glClear(GL_COLOR_BUFFER_BIT);

        GLuint pid = bloomExtractShader->GetProgramID();
        glUseProgram(pid);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        glUniform1i(glGetUniformLocation(pid, "hdrTex"), 0);
        glUniform1f(glGetUniformLocation(pid, "threshold"), params.bloomThreshold);

        DrawFullscreenTriangle();

        // ── 2. Bloom Blur (ping-pong) ───────────────────────────────
        GLuint blurPid = bloomBlurShader->GetProgramID();
        glUseProgram(blurPid);
        int inputTexLoc   = glGetUniformLocation(blurPid, "inputTex");
        int horizontalLoc = glGetUniformLocation(blurPid, "horizontal");

        bool horizontal = true;
        GLuint currentInput = extractTex;

        for (int i = 0; i < params.bloomIterations; ++i) {
            int idx = horizontal ? 0 : 1;
            glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[idx]);
            glClear(GL_COLOR_BUFFER_BIT);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentInput);
            glUniform1i(inputTexLoc, 0);
            glUniform1i(horizontalLoc, horizontal ? 1 : 0);

            DrawFullscreenTriangle();

            currentInput = bloomTex[idx];
            horizontal = !horizontal;
        }
    }

    // ── 3. Tone Map + Bloom Composite + Gamma ───────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint tmPid = tonemapShader->GetProgramID();
    glUseProgram(tmPid);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex);
    glUniform1i(glGetUniformLocation(tmPid, "hdrTex"), 0);

    if (params.enableBloom) {
        // Last written bloom buffer
        GLuint lastBloomTex = bloomTex[(params.bloomIterations % 2 == 0) ? 1 : 0];
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, lastBloomTex);
        glUniform1i(glGetUniformLocation(tmPid, "bloomTex"), 1);
    }

    glUniform1f(glGetUniformLocation(tmPid, "exposure"), params.exposure);
    glUniform1f(glGetUniformLocation(tmPid, "gamma"), params.gamma);
    glUniform1f(glGetUniformLocation(tmPid, "bloomIntensity"), params.bloomIntensity);
    glUniform1i(glGetUniformLocation(tmPid, "enableBloom"), params.enableBloom ? 1 : 0);
    glUniform1i(glGetUniformLocation(tmPid, "enableTonemap"), params.enableTonemap ? 1 : 0);

    DrawFullscreenTriangle();

    // Restore state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glUseProgram(0);
}

void PostProcessPipeline::Destroy() {
    if (!m_Initialized) return;

    DestroyBloomFBOs();

    if (emptyVAO) { glDeleteVertexArrays(1, &emptyVAO); emptyVAO = 0; }

    delete bloomExtractShader; bloomExtractShader = nullptr;
    delete bloomBlurShader;    bloomBlurShader    = nullptr;
    delete tonemapShader;      tonemapShader      = nullptr;

    m_Initialized = false;
}

void PostProcessPipeline::CreateBloomFBOs(int width, int height) {
    m_Width  = width;
    m_Height = height;

    int halfW = width  / 2;
    int halfH = height / 2;

    // Extract FBO (half res)
    glGenTextures(1, &extractTex);
    glBindTexture(GL_TEXTURE_2D, extractTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, halfW, halfH, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &extractFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, extractFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, extractTex, 0);

    // Ping-pong FBOs (half res)
    for (int i = 0; i < 2; ++i) {
        glGenTextures(1, &bloomTex[i]);
        glBindTexture(GL_TEXTURE_2D, bloomTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, halfW, halfH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &bloomFBO[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PostProcessPipeline::DestroyBloomFBOs() {
    if (extractFBO) { glDeleteFramebuffers(1, &extractFBO); extractFBO = 0; }
    if (extractTex) { glDeleteTextures(1, &extractTex);     extractTex = 0; }

    for (int i = 0; i < 2; ++i) {
        if (bloomFBO[i]) { glDeleteFramebuffers(1, &bloomFBO[i]); bloomFBO[i] = 0; }
        if (bloomTex[i]) { glDeleteTextures(1, &bloomTex[i]);     bloomTex[i] = 0; }
    }
}

void PostProcessPipeline::DrawFullscreenTriangle() {
    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

} // namespace ECS
