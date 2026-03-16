#version 400 core
// ── RSM Shadow Pass（Cascade 0，MRT）────────────────────────
// 同时输出 flux（表面颜色 × 光照）、世界空间法线、世界空间位置
// 用于 rsm_indirect.frag 计算单次弹射间接漫反射

layout(location = 0) out vec3 outFlux;   // albedo * sunColour
layout(location = 1) out vec3 outNormal; // 世界空间法线
layout(location = 2) out vec3 outPos;    // 世界空间位置

in vec2 vTexCoord;
in vec3 vNormal; // 从 shadow.vert 传入的世界空间法线
in vec3 vWorldPos;

uniform sampler2D albedoTex;
uniform vec4      objectColour = vec4(1.0);
uniform vec3      sunColour    = vec3(1.0);
uniform bool      hasTexture   = false;

void main() {
    vec3 albedo = objectColour.rgb;
    if (hasTexture) albedo *= texture(albedoTex, vTexCoord).rgb;
    outFlux   = albedo * sunColour;
    outNormal = normalize(vNormal);
    outPos    = vWorldPos;
}
