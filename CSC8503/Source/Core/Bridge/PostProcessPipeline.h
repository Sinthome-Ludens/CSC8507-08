#pragma once

#include "glad/gl.h"

namespace NCL::Rendering {
    class OGLShader;
}

namespace ECS {

struct PostProcessParams {
    float bloomThreshold  = 1.0f;
    float bloomIntensity  = 0.5f;
    int   bloomIterations = 10;
    float exposure        = 1.0f;
    float gamma           = 2.2f;
    bool  enableBloom     = true;
    bool  enableTonemap   = true;
};

class PostProcessPipeline {
public:
    void Init(int width, int height);
    void Resize(int width, int height);
    void Execute(GLuint hdrColorTex, const PostProcessParams& params);
    void Destroy();

private:
    void CreateBloomFBOs(int width, int height);
    void DestroyBloomFBOs();
    void DrawFullscreenTriangle();

    // Shaders
    NCL::Rendering::OGLShader* bloomExtractShader = nullptr;
    NCL::Rendering::OGLShader* bloomBlurShader    = nullptr;
    NCL::Rendering::OGLShader* tonemapShader      = nullptr;

    // Empty VAO for fullscreen triangle
    GLuint emptyVAO = 0;

    // Bloom ping-pong FBOs (half resolution)
    GLuint bloomFBO[2]  = {0, 0};
    GLuint bloomTex[2]  = {0, 0};
    GLuint extractFBO   = 0;
    GLuint extractTex   = 0;

    int m_Width  = 0;
    int m_Height = 0;
    bool m_Initialized = false;
};

} // namespace ECS
