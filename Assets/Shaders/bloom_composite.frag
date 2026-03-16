#version 400 core
// ── Bloom 合成 + SSAO 叠加 ──────────────────────────────────
// 将 Bloom 纹理叠加到 HDR 颜色，同时应用 SSAO 遮蔽

in vec2 vTexCoord;

uniform sampler2D hdrColorTex;  // 原始 HDR 颜色
uniform sampler2D bloomTex;     // 模糊后的 Bloom 贡献
uniform sampler2D ssaoTex;      // SSAO 模糊结果（unit 2）

uniform float bloomStrength = 0.04; // Bloom 强度系数
uniform bool  useSSAO       = true;

out vec4 fragColor;

void main() {
    vec3  hdr    = texture(hdrColorTex, vTexCoord).rgb;
    vec3  bloom  = texture(bloomTex,    vTexCoord).rgb;
    float ao     = useSSAO ? texture(ssaoTex, vTexCoord).r : 1.0;

    // Bloom 叠加
    vec3 combined = hdr + bloom * bloomStrength;

    // SSAO 仅乘以 ambient 项（近似：乘以全部颜色会使高亮区域过暗）
    // 实际 PBR 中 SSAO 只影响 ambient；此处为简化合成，乘以 0.5 权重
    combined *= mix(ao, 1.0, 0.5);

    fragColor = vec4(combined, texture(hdrColorTex, vTexCoord).a);
}
