#version 400 core
// ── IBL 镜面预过滤（GGX 重要性采样）────────────────────────
// 输入：skybox 立方体贴图；输出：不同粗糙度下的预过滤镜面颜色
// 每个 mip 级别对应不同 roughness 值（0=smooth, 1=rough）

in vec2 vTexCoord;

uniform samplerCube environmentMap;
uniform int         face;
uniform float       roughness; // 当前 mip 级别对应的粗糙度

out vec4 fragColor;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;

/// @brief 低差异序列（Hammersley 2D）
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

/// @brief GGX 重要性采样（根据粗糙度生成半程向量）
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float a) {
    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up    = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);
    return normalize(right * H.x + up * H.y + N * H.z);
}

vec3 FaceUVtoDir(int f, vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;
    if      (f == 0) return normalize(vec3( 1.0, -p.y, -p.x));
    else if (f == 1) return normalize(vec3(-1.0, -p.y,  p.x));
    else if (f == 2) return normalize(vec3( p.x,  1.0,  p.y));
    else if (f == 3) return normalize(vec3( p.x, -1.0, -p.y));
    else if (f == 4) return normalize(vec3( p.x, -p.y,  1.0));
    else             return normalize(vec3(-p.x, -p.y, -1.0));
}

void main() {
    vec3 N = FaceUVtoDir(face, vTexCoord);
    vec3 R = N; vec3 V = R; // 简化假设：视线方向 = 法线

    float a            = roughness * roughness;
    vec3  prefilteredColor = vec3(0.0);
    float totalWeight      = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, a);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefilteredColor += texture(environmentMap, L).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }
    fragColor = vec4(prefilteredColor / totalWeight, 1.0);
}
