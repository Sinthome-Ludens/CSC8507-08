#version 400 core

// ── 输出 ──────────────────────────────────────────────────
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 gNormal;   // 视空间法线→HDR FBO Attachment 1（供 SSAO）

// ── 输入接口块 ──────────────────────────────────────────────
in Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
    mat3 TBN;
    vec3 viewNormal;
} IN;

// ── 纹理 ────────────────────────────────────────────────────
uniform sampler2D albedoTex; // unit 0 (albedo/diffuse)
uniform sampler2D normalTex; // unit 1 (切线空间法线贴图)

// ── 阴影贴图（CSM 三级联 unit 5-7，原始深度）─────────────────
uniform sampler2D shadowTex0;
uniform sampler2D shadowTex1;
uniform sampler2D shadowTex2;
uniform mat4      shadowMatrix0;
uniform mat4      shadowMatrix1;
uniform mat4      shadowMatrix2;
uniform float     cascadeSplits[3]; // 视空间距离切分 {15, 60, 300}

// ── 光照 ────────────────────────────────────────────────────
uniform vec3  sunPos;
uniform float sunRadius;
uniform vec3  sunColour;
uniform vec3  cameraPos;

// ── 材质开关 ────────────────────────────────────────────────
uniform bool  hasTexture = false;
uniform bool  hasBumpTex = false;

// ── 边缘高亮（rim lighting）参数──已由当前分支实现，保持不变
uniform vec3  rimColour   = vec3(0.0, 0.0, 0.0);
uniform float rimPower    = 3.0;
uniform float rimStrength = 0.0;

// ── PCSS ────────────────────────────────────────────────────
uniform float pcssLightSize = 3.0;

// ── Alpha ────────────────────────────────────────────────────
uniform int   alphaMode   = 0;
uniform float alphaCutoff = 0.5;
uniform bool  doubleSided = false;

// ============================================================
// PCSS 阴影辅助
// ============================================================
const vec2 POISSON12[12] = vec2[12](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696,  0.457),
    vec2(-0.203,  0.621), vec2( 0.962, -0.195), vec2( 0.473, -0.480),
    vec2( 0.519,  0.767), vec2( 0.185, -0.893), vec2( 0.507,  0.064),
    vec2( 0.896,  0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598)
);
const vec2 POISSON25[25] = vec2[25](
    vec2(-0.978, -0.101), vec2(-0.863,  0.353), vec2(-0.622, -0.654),
    vec2(-0.498, -0.173), vec2(-0.518,  0.589), vec2(-0.265, -0.889),
    vec2(-0.163, -0.484), vec2(-0.198,  0.311), vec2(-0.116,  0.736),
    vec2( 0.111, -0.204), vec2( 0.236, -0.685), vec2( 0.108,  0.169),
    vec2( 0.289,  0.582), vec2( 0.388, -0.339), vec2( 0.563,  0.051),
    vec2( 0.529, -0.818), vec2( 0.545,  0.765), vec2( 0.674,  0.332),
    vec2( 0.793, -0.147), vec2( 0.864,  0.631), vec2( 0.843, -0.587),
    vec2( 0.968,  0.124), vec2(-0.763,  0.855), vec2(-0.423,  0.887),
    vec2( 0.142,  0.972)
);

float SampleShadowCSM(sampler2D shadowMap, mat4 shadowMat, vec3 worldPos,
                      int mapSize, bool simplePCF) {
    vec4 shadowCoord = shadowMat * vec4(worldPos, 1.0);
    if (shadowCoord.w <= 0.0) return 1.0;
    vec3 proj = shadowCoord.xyz / shadowCoord.w;
    if (proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0) return 1.0;
    float depth = proj.z;

    if (simplePCF) {
        float s = 0.0; float ts = 1.0 / float(mapSize);
        for (int x = -1; x <= 1; x++) for (int y = -1; y <= 1; y++) {
            s += (texture(shadowMap, proj.xy + vec2(x, y) * ts).r < depth - 0.0003) ? 0.0 : 1.0;
        }
        return s / 9.0;
    }
    float sr = pcssLightSize / float(mapSize);
    float bs = 0.0; int bc = 0;
    for (int i = 0; i < 12; i++) {
        float d = texture(shadowMap, proj.xy + POISSON12[i] * sr).r;
        if (d < depth - 0.0003) { bs += d; bc++; }
    }
    if (bc == 0) return 1.0;
    float fr = clamp((depth - bs / float(bc)) / (bs / float(bc)) * pcssLightSize / float(mapSize),
                     1.0 / float(mapSize), 0.02);
    float s = 0.0;
    for (int i = 0; i < 25; i++) {
        s += (texture(shadowMap, proj.xy + POISSON25[i] * fr).r < depth - 0.0003) ? 0.0 : 1.0;
    }
    return s / 25.0;
}

// ============================================================
// Main
// ============================================================
void main() {
    // ── Albedo ───────────────────────────────────────────────
    vec4 albedo = IN.colour;
    if (hasTexture) albedo *= texture(albedoTex, IN.texCoord);
    if (alphaMode == 1 && albedo.a < alphaCutoff) discard; // Alpha Mask

    // ── 法线 ────────────────────────────────────────────────
    vec3 N;
    if (hasBumpTex) {
        vec3 ns = texture(normalTex, IN.texCoord).rgb * 2.0 - 1.0;
        N = normalize(IN.TBN * ns);
    } else {
        N = normalize(IN.normal);
    }
    if (doubleSided && !gl_FrontFacing) N = -N;

    // ── 光照 ─────────────────────────────────────────────────
    vec3  incident = normalize(sunPos - IN.worldPos);
    float lambert  = max(0.0, dot(incident, N)) * 0.9;
    vec3  viewDir  = normalize(cameraPos - IN.worldPos);
    vec3  halfDir  = normalize(incident + viewDir);
    float sFactor  = pow(max(0.0, dot(halfDir, N)), 80.0);

    // ── CSM 阴影 ─────────────────────────────────────────────
    float viewDist = length(cameraPos - IN.worldPos);
    float shadow;
    if (viewDist < cascadeSplits[0]) {
        shadow = SampleShadowCSM(shadowTex0, shadowMatrix0, IN.worldPos, 4096, false);
    } else if (viewDist < cascadeSplits[1]) {
        shadow = SampleShadowCSM(shadowTex1, shadowMatrix1, IN.worldPos, 2048, false);
    } else {
        shadow = SampleShadowCSM(shadowTex2, shadowMatrix2, IN.worldPos, 1024, true);
    }

    // ── 颜色组合 ─────────────────────────────────────────────
    albedo.rgb = pow(albedo.rgb, vec3(2.2)); // sRGB → Linear（HDR FBO 存线性值）

    fragColor.rgb  = albedo.rgb * 0.05;
    fragColor.rgb += albedo.rgb * sunColour * lambert * shadow;
    fragColor.rgb += sunColour * sFactor * shadow;

    // ── Rim lighting（已由当前分支实现，保持不变）────────────
    if (rimStrength > 0.0) {
        float rimDot = 1.0 - max(dot(viewDir, N), 0.0);
        float rim    = pow(rimDot, rimPower) * rimStrength;
        fragColor.rgb += rimColour * rim;
    }

    // 不在此处做 gamma（HDR FBO 是线性空间，tonemap.frag 统一处理 ACES+gamma）
    fragColor.a   = albedo.a;

    // ── G-Buffer 法线（视空间，供 SSAO）───────────────────────
    gNormal = normalize(IN.viewNormal) * 0.5 + 0.5;
}
