#version 400 core
// ── SSAO 双边模糊（深度感知）──────────────────────────────
// 对 SSAO 原始结果进行 4×4 均值模糊，忽略深度差异过大的邻居

in vec2 vTexCoord;

uniform sampler2D ssaoRawTex;  // SSAO 原始结果（unit 0）
uniform sampler2D depthTex;    // 深度（用于双边权重）
uniform vec2      texelSize;   // = 1.0 / screenSize

// 深度差异阈值（超过此值的邻居权重降为 0）
uniform float depthThreshold = 0.05;

out float fragColor;

void main() {
    float centerDepth = texture(depthTex, vTexCoord).r;
    float result      = 0.0;
    float weightSum   = 0.0;

    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2  offset = vec2(float(x), float(y)) * texelSize;
            vec2  sUV    = vTexCoord + offset;
            float nDepth = texture(depthTex, sUV).r;
            float weight = (abs(nDepth - centerDepth) < depthThreshold) ? 1.0 : 0.0;
            result      += texture(ssaoRawTex, sUV).r * weight;
            weightSum   += weight;
        }
    }
    fragColor = (weightSum > 0.0) ? result / weightSum : 1.0;
}
