#version 400 core
// ── Tone Mapping（ACES Narkowicz 近似）+ Gamma + Vignette ──

in vec2 vTexCoord;

uniform sampler2D hdrTex;        // 合成后的 HDR 颜色
uniform float     exposure       = 1.0;
uniform float     vignetteStrength = 0.3;

out vec4 fragColor;

/// @brief Narkowicz ACES 色调映射近似（快速，广泛使用）
vec3 ACES(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrTex, vTexCoord).rgb;

    // 曝光调节
    hdr *= exposure;

    // ACES 色调映射
    vec3 mapped = ACES(hdr);

    // Gamma 校正（线性→sRGB）
    mapped = pow(mapped, vec3(1.0 / 2.2));

    // Vignette（径向暗角）
    float d = length(vTexCoord - 0.5);
    float vignette = 1.0 - d * d * vignetteStrength;
    mapped *= vignette;

    fragColor = vec4(mapped, 1.0);
}
