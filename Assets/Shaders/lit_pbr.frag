#version 400 core

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

// ── IBL（unit 8-10，Stage 3 接入后取消注释）─────────────────
// uniform samplerCube irradianceMap;  // unit 8
// uniform samplerCube prefilterMap;   // unit 9
// uniform sampler2D   brdfLUT;        // unit 10
uniform float iblIntensity = 1.0;
uniform bool  useIBL       = false;

// ── 光照 ────────────────────────────────────────────────────
uniform vec3  sunPos;
uniform vec3  sunColour = vec3(1.0, 1.0, 1.0);
uniform vec3  cameraPos;

// ── 材质参数（标量 fallback，ORM 贴图优先）────────────────────
uniform float u_metallic  = 0.0;
uniform float u_roughness = 0.5;
uniform float u_ao        = 1.0;
uniform vec3  u_emissiveColor    = vec3(0.0);
uniform float u_emissiveStrength = 0.0;

// ── Alpha 模式 ──────────────────────────────────────────────
// 0=Opaque, 1=Mask, 2=Blend
uniform int   alphaMode   = 0;
uniform float alphaCutoff = 0.5;
uniform bool  doubleSided = false;

// ── PCSS 参数 ────────────────────────────────────────────────
uniform float pcssLightSize = 3.0; // 光源尺寸（光照空间单位）

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

/// @brief PCSS: 搜索遮挡物平均深度
float FindBlockerAvgDepth(sampler2D shadowMap, vec2 uv, float depth, float searchRadius) {
    float blockerSum = 0.0;
    int   blockerCount = 0;
    for (int i = 0; i < 12; i++) {
        vec2 offset = POISSON12[i] * searchRadius;
        float shadowDepth = texture(shadowMap, uv + offset).r;
        if (shadowDepth < depth - 0.005) {
            blockerSum += shadowDepth;
            blockerCount++;
        }
    }
    if (blockerCount == 0) return -1.0; // 无遮挡物
    return blockerSum / float(blockerCount);
}

/// @brief PCSS: PCF 采样（可变半径）
float PCFFilter(sampler2D shadowMap, vec2 uv, float depth, float filterRadius) {
    float shadow = 0.0;
    for (int i = 0; i < 25; i++) {
        vec2 offset = POISSON25[i] * filterRadius;
        float shadowDepth = texture(shadowMap, uv + offset).r;
        shadow += (shadowDepth < depth - 0.005) ? 0.0 : 1.0;
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
                shadow += (d < depth - 0.005) ? 0.0 : 1.0;
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

/// @brief 根据视空间深度选择 CSM 级联并采样阴影
float ComputeCascadeShadow(vec3 worldPos, float viewDepth) {
    mat4  chosenMatrix;
    sampler2D chosenTex;
    int   chosenRes;
    bool  simplePCF;

    if (viewDepth < cascadeSplits[0]) {
        chosenMatrix = shadowMatrix0;
        chosenTex    = shadowTex0;
        chosenRes    = 4096;
        simplePCF    = false;
    } else if (viewDepth < cascadeSplits[1]) {
        chosenMatrix = shadowMatrix1;
        chosenTex    = shadowTex1;
        chosenRes    = 2048;
        simplePCF    = false;
    } else {
        chosenMatrix = shadowMatrix2;
        chosenTex    = shadowTex2;
        chosenRes    = 1024;
        simplePCF    = true;
    }

    vec4 shadowCoord = chosenMatrix * vec4(worldPos, 1.0);
    return SampleShadowPCSS(chosenTex, shadowCoord, chosenRes, simplePCF);
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
    float ao        = ormSample.r  * u_ao;      // R = occlusion
    float roughness = ormSample.g  + u_roughness * (1.0 - ormSample.g); // G = roughness
    float metallic  = ormSample.b  + u_metallic  * (1.0 - ormSample.b); // B = metallic
    roughness = clamp(roughness, 0.04, 1.0);
    metallic  = clamp(metallic,  0.0,  1.0);

    // ── 视方向 ──────────────────────────────────────────────
    vec3 V = normalize(cameraPos - IN.worldPos);

    // ── F0（反射率基础值）────────────────────────────────────
    vec3 albedo = albedoSample.rgb;
    albedo = pow(albedo, vec3(2.2)); // sRGB → Linear
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── 视空间深度（用于 CSM 级联选择）──────────────────────
    // 近似：world→view 的 z 分量
    // 实际用 gl_FragCoord.z 重建深度会更准确，但这里用简化版
    // float viewZ = length(cameraPos - IN.worldPos); // 等效于 view-space depth 近似
    // 更准确的 viewDepth：
    float viewDepth = abs(dot(normalize(cameraPos - IN.worldPos), N)); // placeholder
    // 真正的 viewDepth 需从相机矩阵计算，这里使用近似
    // 在 C++ 端通过 uniform 传入视空间 Z 值会更精确，此处用 worldPos 距离代替
    viewDepth = length(cameraPos - IN.worldPos);

    // ── 太阳光直接照明（Cook-Torrance BRDF）────────────────
    vec3 Lo = vec3(0.0);
    {
        vec3 L = normalize(sunPos - IN.worldPos); // 点光源；若定向光则 L = normalize(sunPos)
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.001);
        float NdotH = max(dot(N, H), 0.0);

        // Cook-Torrance 镜面 BRDF
        float D = D_GGX(NdotH, roughness * roughness);
        float G = G_Smith(NdotV, NdotL, roughness);
        vec3  F = F_Schlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = D * G * F;
        float denominator = 4.0 * NdotV * NdotL + 0.0001;
        vec3  specular    = numerator / denominator;

        // kD（漫反射权重，金属无漫反射）
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);

        // 阴影
        float shadow = ComputeCascadeShadow(IN.worldPos, viewDepth);

        Lo += (kD * albedo / PI + specular) * sunColour * NdotL * shadow;
    }

    // ── IBL 环境光（Stage 3 接通后取消注释）─────────────────
    vec3 ambient = vec3(0.03) * albedo * ao;
    /*
    if (useIBL) {
        float NdotV = max(dot(N, V), 0.0);
        vec3  F = F_SchlickRoughness(NdotV, F0, roughness);
        vec3  kS = F;
        vec3  kD = (1.0 - kS) * (1.0 - metallic);

        vec3  irradiance = texture(irradianceMap, N).rgb;
        vec3  diffIBL    = kD * irradiance * albedo;

        const float MAX_REFLECTION_LOD = 4.0;
        vec3  R = reflect(-V, N);
        vec3  prefiltered = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2  envBRDF     = texture(brdfLUT, vec2(NdotV, roughness)).rg;
        vec3  specIBL     = prefiltered * (F * envBRDF.x + envBRDF.y);

        ambient = (diffIBL + specIBL) * ao * iblIntensity;
    }
    */

    // ── 自发光 ────────────────────────────────────────────────
    vec3 emissive = texture(emissiveTex, IN.texCoord).rgb * u_emissiveColor * u_emissiveStrength;

    // ── 最终颜色（HDR，后续由 tonemap.frag 处理）───────────────
    vec3 finalColor = ambient + Lo + emissive;

    fragColor = vec4(finalColor, albedoSample.a);

    // ── G-Buffer 法线输出（视空间法线，pack 到 [0,1]）──────────
    gNormal = normalize(IN.viewNormal) * 0.5 + 0.5;
}
