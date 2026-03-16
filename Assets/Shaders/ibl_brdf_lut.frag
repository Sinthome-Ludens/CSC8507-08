#version 400 core
// ── IBL BRDF LUT 预积分 ─────────────────────────────────────
// 输出：512×512 2D 纹理
// X 轴 = NdotV（[0,1]），Y 轴 = roughness（[0,1]）
// 输出 RG 通道 = (scale, bias)，用于分离和 BRDF：
//   F_env = F0 * scale + bias

in vec2 vTexCoord; // NdotV = x, roughness = y

out vec2 fragColor;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;

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

float G_SchlickGGX(float NdotV, float a) {
    float k = a / 2.0; // IBL 使用 k = a/2（与直接光照的 k 不同）
    return NdotV / (NdotV * (1.0 - k) + k);
}
float G_Smith(float NdotV, float NdotL, float a) {
    return G_SchlickGGX(NdotV, a) * G_SchlickGGX(NdotL, a);
}

void main() {
    float NdotV   = vTexCoord.x;
    float roughness = vTexCoord.y;
    float a       = roughness * roughness;

    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    vec3 N = vec3(0.0, 0.0, 1.0);

    float A = 0.0, B = 0.0;
    for (uint i = 0u; i < SAMPLE_COUNT; i++) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, a);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        if (NdotL > 0.0) {
            float G   = G_Smith(NdotV, NdotL, a);
            float GVis = (G * VdotH) / (NdotH * NdotV + 0.0001);
            float Fc  = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * GVis;
            B += Fc * GVis;
        }
    }
    fragColor = vec2(A, B) / float(SAMPLE_COUNT);
}
