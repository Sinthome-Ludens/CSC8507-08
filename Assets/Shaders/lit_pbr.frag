#version 430 core

// ── 输出 ──────────────────────────────────────────────────
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 gNormal;   // 视空间法线写入 HDR FBO Attachment 1（供 SSAO）

// ── 输入接口块 ──────────────────────────────────────────────
in Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 worldPos;
    vec3 normal;
    mat3 TBN;
    vec3 viewNormal;
} IN;

// ── 材质纹理 ─────────────────────────────────────────────────
uniform sampler2D albedoTex;    // unit 0
uniform sampler2D normalTex;    // unit 1  (切线空间法线贴图)
uniform sampler2D ormTex;       // unit 2  (R=occlusion G=roughness B=metallic)
uniform sampler2D emissiveTex;  // unit 3

// ── 阴影贴图（CSM 三级联，unit 5-7）─────────────────────────
uniform sampler2D shadowTex0;   // cascade 0（近）
uniform sampler2D shadowTex1;   // cascade 1（中）
uniform sampler2D shadowTex2;   // cascade 2（远）
uniform mat4 shadowMatrix0;
uniform mat4 shadowMatrix1;
uniform mat4 shadowMatrix2;
uniform float cascadeSplits[3]; // = {15, 60, 300}（视空间 z）

// ── IBL（unit 8-10）─────────────────────────────────────────
uniform samplerCube irradianceMap;  // unit 8
uniform samplerCube prefilterMap;   // unit 9
uniform sampler2D   brdfLUT;        // unit 10
uniform float iblIntensity = 1.0;
uniform bool  useIBL       = false;

// ── 光照 ────────────────────────────────────────────────────
uniform vec3  sunPos;
uniform vec3  sunColour = vec3(1.0, 1.0, 1.0);
uniform vec3  cameraPos;

// ── 材质参数（标量 fallback，ORM 贴图优先）────────────────────
uniform float metallic  = 0.0;
uniform float roughness = 0.5;
uniform float ao        = 1.0;
uniform vec3  emissiveColor    = vec3(0.0);
uniform float emissiveStrength = 0.0;

// ── Alpha 模式 ──────────────────────────────────────────────
// 0=Opaque, 1=Mask, 2=Blend
uniform int   alphaMode   = 0;
uniform float alphaCutoff = 0.5;
uniform bool  doubleSided = false;

// ── PCSS 参数 ────────────────────────────────────────────────
uniform float pcssLightSize = 3.0; // 光源尺寸（光照空间单位）

// ── 阴影偏置（可通过 ImGui 动态调整）────────────────────────
uniform float shadowBiasSlope    = 0.0001; // tan(theta) 系数：掠射面自动增大
uniform float shadowBiasConstant = 0.00005; // 基础常量偏置：避免 contact shadow 失效

// ── 斜率自适应阴影偏置（在 main() 中基于 N·L 设定，helper 函数直接引用）──
// 正对光时接近零，掠射角时增大，防止自阴影 acne 同时避免 Peter Panning
float g_shadowBias = 0.0003;

// ============================================================
// PBR 辅助函数
// ============================================================

const float PI = 3.14159265359;

/// @brief GGX 正态分布函数
float D_GGX(float NdotH, float a) {
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

/// @brief Smith 几何遮蔽函数（Schlick-GGX）
float G_SchlickGGX(float NdotV, float a) {
    float k = (a + 1.0) * (a + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float a) {
    return G_SchlickGGX(NdotV, a) * G_SchlickGGX(NdotL, a);
}

/// @brief Schlick Fresnel 近似
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/// @brief Schlick Fresnel（考虑粗糙度，用于 IBL 环境光）
vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================
// PCSS 辅助（读原始深度，不用 shadow compare）
// ============================================================

/// @brief Poisson 磁盘采样点（12 点，用于 Blocker Search）
const vec2 POISSON12[12] = vec2[12](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074), vec2(-0.696,  0.457),
    vec2(-0.203,  0.621), vec2( 0.962, -0.195), vec2( 0.473, -0.480),
    vec2( 0.519,  0.767), vec2( 0.185, -0.893), vec2( 0.507,  0.064),
    vec2( 0.896,  0.412), vec2(-0.322, -0.933), vec2(-0.792, -0.598)
);

/// @brief Poisson 磁盘采样点（25 点，用于 PCF Filter）
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

/// @brief Interleaved Gradient Noise → [0, 2π)，每像素唯一旋转角，消除固定 Poisson 图样
float IGNAngle() {
    return fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) * 6.28318530718;
}

/// @brief PCSS: 搜索遮挡物平均深度
float FindBlockerAvgDepth(sampler2D shadowMap, vec2 uv, float depth, float searchRadius) {
    float angle = IGNAngle();
    float cs = cos(angle), sn = sin(angle);
    float blockerSum = 0.0;
    int   blockerCount = 0;
    for (int i = 0; i < 12; i++) {
        vec2 p = vec2(cs * POISSON12[i].x - sn * POISSON12[i].y,
                      sn * POISSON12[i].x + cs * POISSON12[i].y) * searchRadius;
        float shadowDepth = texture(shadowMap, uv + p).r;
        if (shadowDepth < depth - g_shadowBias) {
            blockerSum += shadowDepth;
            blockerCount++;
        }
    }
    if (blockerCount == 0) return -1.0; // 无遮挡物
    return blockerSum / float(blockerCount);
}

/// @brief PCSS: PCF 采样（可变半径）
float PCFFilter(sampler2D shadowMap, vec2 uv, float depth, float filterRadius) {
    float angle = IGNAngle();
    float cs = cos(angle), sn = sin(angle);
    float shadow = 0.0;
    for (int i = 0; i < 25; i++) {
        vec2 p = vec2(cs * POISSON25[i].x - sn * POISSON25[i].y,
                      sn * POISSON25[i].x + cs * POISSON25[i].y) * filterRadius;
        float shadowDepth = texture(shadowMap, uv + p).r;
        shadow += (shadowDepth < depth - g_shadowBias) ? 0.0 : 1.0;
    }
    return shadow / 25.0;
}

/// @brief PCSS 阴影（返回 [0,1]，0=完全遮蔽）
float SampleShadowPCSS(sampler2D shadowMap, vec4 shadowCoord, int mapSize, bool simplePCF) {
    if (shadowCoord.w <= 0.0) return 1.0;
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0)
        return 1.0;

    float depth = projCoords.z;

    // C2（远级联）退化为简单 3×3 PCF，节省 GPU
    if (simplePCF) {
        float shadow = 0.0;
        float texelSize = 1.0 / float(mapSize);
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                float d = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += (d < depth - g_shadowBias) ? 0.0 : 1.0;
            }
        }
        return shadow / 9.0;
    }

    // Step 1: Blocker Search
    float searchRadius = pcssLightSize / float(mapSize);
    float avgBlocker = FindBlockerAvgDepth(shadowMap, projCoords.xy, depth, searchRadius);
    if (avgBlocker < 0.0) return 1.0; // 完全无遮挡

    // Step 2: Penumbra Width
    float penumbra = (depth - avgBlocker) / avgBlocker * pcssLightSize;

    // Step 3: PCF
    float filterRadius = penumbra / float(mapSize);
    filterRadius = clamp(filterRadius, 1.0 / float(mapSize), 0.02);
    return PCFFilter(shadowMap, projCoords.xy, depth, filterRadius);
}

/// @brief 根据视空间深度选择 CSM 级联并采样阴影，过渡区双采样混合消除硬缝锯齿
float ComputeCascadeShadow(vec3 worldPos, float viewDepth) {
    // 相邻级联过渡混合宽度（世界单位）；在此区间内双倍采样并插值
    const float blendRange = 4.0;

    // 保存原始 bias，按级联 texel 密度缩放后恢复
    float baseBias = g_shadowBias;

    vec4 sc0 = shadowMatrix0 * vec4(worldPos, 1.0);
    vec4 sc1 = shadowMatrix1 * vec4(worldPos, 1.0);
    vec4 sc2 = shadowMatrix2 * vec4(worldPos, 1.0);

    if (viewDepth < cascadeSplits[0]) {
        g_shadowBias = baseBias;          // C0 (4096): 1.0x
        float s0 = SampleShadowPCSS(shadowTex0, sc0, 4096, false);
        // 接近 C0→C1 边界：与 C1 混合
        float blendStart = cascadeSplits[0] - blendRange;
        if (viewDepth > blendStart) {
            float t  = (viewDepth - blendStart) / blendRange;
            g_shadowBias = baseBias * 2.0; // C1 (2048): 2.0x
            float s1 = SampleShadowPCSS(shadowTex1, sc1, 2048, false);
            g_shadowBias = baseBias;
            return mix(s0, s1, t);
        }
        g_shadowBias = baseBias;
        return s0;
    } else if (viewDepth < cascadeSplits[1]) {
        g_shadowBias = baseBias * 2.0;    // C1 (2048): 2.0x
        float s1 = SampleShadowPCSS(shadowTex1, sc1, 2048, false);
        // 接近 C1→C2 边界：与 C2 混合
        float blendStart = cascadeSplits[1] - blendRange;
        if (viewDepth > blendStart) {
            float t  = (viewDepth - blendStart) / blendRange;
            g_shadowBias = baseBias * 4.0; // C2 (1024): 4.0x
            float s2 = SampleShadowPCSS(shadowTex2, sc2, 1024, true);
            g_shadowBias = baseBias;
            return mix(s1, s2, t);
        }
        g_shadowBias = baseBias;
        return s1;
    } else {
        g_shadowBias = baseBias * 4.0;    // C2 (1024): 4.0x
        float result = SampleShadowPCSS(shadowTex2, sc2, 1024, true);
        g_shadowBias = baseBias;
        return result;
    }
}

// ============================================================
// Main
// ============================================================
void main() {
    // ── Alpha 처리 ────────────────────────────────────────────
    vec4 albedoSample = texture(albedoTex, IN.texCoord) * IN.colour;
    if (alphaMode == 1 && albedoSample.a < alphaCutoff) discard; // Mask

    // ── 法线（切线空间→世界空间）────────────────────────────
    vec3 normalSample = texture(normalTex, IN.texCoord).rgb;
    normalSample = normalSample * 2.0 - 1.0; // [0,1] → [-1,1]
    vec3 N = normalize(IN.TBN * normalSample);
    if (doubleSided && !gl_FrontFacing) N = -N;

    // ── ORM 贴图 ────────────────────────────────────────────
    vec3  ormSample = texture(ormTex, IN.texCoord).rgb;
    float matAo        = ormSample.r  * ao;          // R = occlusion
    float matRoughness = ormSample.g  + roughness * (1.0 - ormSample.g); // G = roughness
    float matMetallic  = ormSample.b  + metallic  * (1.0 - ormSample.b); // B = metallic
    matRoughness = clamp(matRoughness, 0.04, 1.0);
    matMetallic  = clamp(matMetallic,  0.0,  1.0);

    // ── 视方向 ──────────────────────────────────────────────
    vec3 V = normalize(cameraPos - IN.worldPos);

    // ── F0（反射率基础值）────────────────────────────────────
    vec3 albedo = albedoSample.rgb;
    albedo = pow(albedo, vec3(2.2)); // sRGB → Linear
    vec3 F0 = mix(vec3(0.04), albedo, matMetallic);

    // ── 视空间深度（CSM 级联选择用摄像机距离近似）──────────
    float viewDepth = length(cameraPos - IN.worldPos);

    // ── 太阳光直接照明（Cook-Torrance BRDF）────────────────
    vec3 Lo = vec3(0.0);
    {
        vec3 L = normalize(sunPos); // 方向光：sunPos 实际为光照方向向量
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.001);
        float NdotH = max(dot(N, H), 0.0);

        // Cook-Torrance 镜面 BRDF
        float D = D_GGX(NdotH, matRoughness * matRoughness);
        float G = G_Smith(NdotV, NdotL, matRoughness);
        vec3  F = F_Schlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        vec3  specular    = numerator / denominator;

        // kD（漫反射权重，金属无漫反射）
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - matMetallic);

        // 斜率自适应 shadow bias：tan(theta) = sinTheta/NdotL，与表面坡度成正比
        // GL_FRONT culling 已消除 light-facing 面自阴影；此 bias 防止背光侧邻接面 acne
        float NdotL_bias = max(dot(N, L), 0.0);
        float sinAlpha   = sqrt(max(0.0, 1.0 - NdotL_bias * NdotL_bias));
        float tanAlpha   = sinAlpha / max(NdotL_bias, 0.01);
        g_shadowBias = clamp(shadowBiasSlope * tanAlpha + shadowBiasConstant, shadowBiasConstant, 0.005);

        // 阴影
        float shadow = ComputeCascadeShadow(IN.worldPos, viewDepth);

        Lo += (kD * albedo / PI + specular) * sunColour * NdotL * shadow;
    }

    // ── IBL 环境光 ─────────────────────────────────────────────
    vec3 ambient = vec3(0.03) * albedo * matAo;
    if (useIBL) {
        float NdotV = max(dot(N, V), 0.0);
        vec3  F = F_SchlickRoughness(NdotV, F0, roughness);
        vec3  kS = F;
        vec3  kD = (1.0 - kS) * (1.0 - matMetallic);

        vec3  irradiance = texture(irradianceMap, N).rgb;
        vec3  diffIBL    = kD * irradiance * albedo;

        const float MAX_REFLECTION_LOD = 4.0;
        vec3  R = reflect(-V, N);
        vec3  prefiltered = textureLod(prefilterMap, R, matRoughness * MAX_REFLECTION_LOD).rgb;
        vec2  envBRDF     = texture(brdfLUT, vec2(NdotV, matRoughness)).rg;
        vec3  specIBL     = prefiltered * (F * envBRDF.x + envBRDF.y);

        ambient = (diffIBL + specIBL) * matAo * iblIntensity;
    }

    // ── 自发光 ────────────────────────────────────────────────
    vec3 emissive = texture(emissiveTex, IN.texCoord).rgb * emissiveColor * emissiveStrength;

    // ── 最终颜色（HDR，后续由 tonemap.frag 处理）───────────────
    vec3 finalColor = ambient + Lo + emissive;

    fragColor = vec4(finalColor, albedoSample.a);

    // ── G-Buffer 法线输出（视空间法线，pack 到 [0,1]）──────────
    gNormal = normalize(IN.viewNormal) * 0.5 + 0.5;
}
