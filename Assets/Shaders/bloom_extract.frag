#version 400 core
// ── Bloom 提取 Pass ──────────────────────────────────────
// 从 HDR 颜色缓冲中提取亮度 > threshold 的部分

in vec2 vTexCoord;

uniform sampler2D hdrColorTex; // HDR 颜色输入
uniform float     bloomThreshold = 1.0;

out vec4 fragColor;

// 亮度权重（Rec.709）
float Luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec3 color = texture(hdrColorTex, vTexCoord).rgb;
    float luma = Luminance(color);
    // 软阈值（避免硬切）
    float brightness = max(luma - bloomThreshold, 0.0);
    float scale = brightness / (luma + 0.0001);
    fragColor = vec4(color * scale, 1.0);
}
