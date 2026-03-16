#version 400 core
// ── RSM 间接漫反射（Reflective Shadow Map）──────────────────
// 以 1/4 分辨率运行，读取 RSM 纹理，计算单次弹射间接光贡献
// 结果叠加到 PBR ambient 项（同 SSAO 叠加方式）

in vec2 vTexCoord;

// RSM 纹理（由 shadow pass MRT 写入）
uniform sampler2D rsmFluxTex;   // RGB = albedo * sunColour（flux）
uniform sampler2D rsmNormalTex; // RGB = world normal（归一化）
uniform sampler2D rsmPosTex;    // RGB = world position

// 当前片元信息（从 G-buffer 重建）
uniform sampler2D scenePosTexW; // world pos（由 C++ 写入 HDR FBO 的附加 attachment）
uniform sampler2D sceneNormal;  // world normal（同上）
uniform sampler2D sceneDepth;   // 深度（用于遮挡）

// RSM 参数
uniform float rsmRadius   = 0.1; // 光照空间采样半径
uniform float rsmStrength = 1.0; // 间接光强度系数
uniform int   nRSMSamples = 32;  // 采样数量

// 随机采样种子（Poisson disk，预先由 C++ 传入）
uniform vec2  rsmSamples[64];    // 最多 64 个采样点

out vec4 fragColor;

const float PI = 3.14159265359;

void main() {
    // 获取当前像素的世界空间位置和法线
    vec3 xp = texture(scenePosTexW, vTexCoord).rgb;
    vec3 np = normalize(texture(sceneNormal, vTexCoord).rgb * 2.0 - 1.0);

    vec3 indirect = vec3(0.0);
    int  count    = min(nRSMSamples, 64);

    for (int i = 0; i < count; i++) {
        // 在 RSM 空间随机采样
        vec2 offset = rsmSamples[i] * rsmRadius;
        vec2 sampleUV = vTexCoord + offset;
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0) continue;

        vec3 xq  = texture(rsmPosTex,   sampleUV).rgb; // 采样点世界位置
        vec3 nq  = texture(rsmNormalTex, sampleUV).rgb; // 采样点法线
        vec3 Phi = texture(rsmFluxTex,   sampleUV).rgb; // 采样点 flux

        vec3 delta = xp - xq;
        float dist2 = dot(delta, delta) + 0.0001;

        // RSM 贡献公式（Dachsbacher & Stamminger）
        // E_i = Phi * max(0, dot(nq, delta)) * max(0, dot(np, -delta)) / dist2^2
        float contrib = max(0.0, dot(nq, normalize(delta))) *
                        max(0.0, dot(np, normalize(-delta)));
        // 权重 = ||delta||^2（使近处样本贡献大）
        indirect += Phi * contrib * length(rsmSamples[i] * rsmRadius) * length(rsmSamples[i] * rsmRadius) / dist2;
    }

    indirect *= rsmStrength / float(count);
    fragColor  = vec4(indirect, 1.0);
}
