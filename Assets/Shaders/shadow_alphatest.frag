#version 400 core

// ── Shadow Alpha-Test Pass ─────────────────────────────────
// 用于 Alpha-Mask 物体的 shadow cast：采样 albedo，低于阈值则丢弃

in vec2 vTexCoord;

uniform sampler2D albedoTex;
uniform float     alphaCutoff = 0.5;

void main() {
    vec4 albedo = texture(albedoTex, vTexCoord);
    if (albedo.a < alphaCutoff) discard;
    // 深度值由 OpenGL 自动写入，无需显式输出
}
