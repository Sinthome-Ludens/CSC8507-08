#version 400 core
// ── Bloom Kawase 模糊（下采样 + 上采样共用）─────────────────
// 传统双线性加权 Kawase kernel，比高斯更省带宽

in vec2 vTexCoord;

uniform sampler2D bloomTex;  // 输入：上一 pass 的结果
uniform vec2      texelSize; // = 1.0 / inputSize
uniform int       iteration; // 当前 Kawase 迭代次数（0,1,2,...）

out vec4 fragColor;

void main() {
    float offset = float(iteration) + 0.5;
    vec4 sum = vec4(0.0);
    sum += texture(bloomTex, vTexCoord + vec2(-offset,  offset) * texelSize);
    sum += texture(bloomTex, vTexCoord + vec2( offset,  offset) * texelSize);
    sum += texture(bloomTex, vTexCoord + vec2(-offset, -offset) * texelSize);
    sum += texture(bloomTex, vTexCoord + vec2( offset, -offset) * texelSize);
    fragColor = sum * 0.25;
}
