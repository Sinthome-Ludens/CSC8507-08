#version 400 core
// ── SSAO 计算 Pass ────────────────────────────────────────
// 输入：线性深度纹理 + 视空间法线纹理 + 噪声纹理（4×4）
// 输出：遮蔽值 [0,1]（1=完全无遮蔽，0=完全遮蔽）
// 以全屏三角形运行（fullscreen.vert）

in vec2 vTexCoord;

uniform sampler2D depthTex;     // 场景线性深度（unit 0）
uniform sampler2D normalTex;    // 视空间法线 pack [0,1]（unit 1）
uniform sampler2D noiseTex;     // 4×4 RGBA16F 旋转噪声（unit 2）

// 相机参数（用于深度→视空间位置重建）
uniform mat4 projMatrix;
uniform mat4 invProjMatrix;
uniform vec2 screenSize;        // 像素分辨率

// SSAO 参数
uniform vec3  ssaoKernel[64];   // 半球采样核（CPU 预计算）
uniform int   kernelSize  = 64;
uniform float ssaoRadius  = 0.5;
uniform float ssaoBias    = 0.025;

out float fragColor; // 单通道遮蔽值

/// @brief 从深度值和 NDC texcoord 重建视空间位置
vec3 ReconstructPosition(float depth, vec2 texCoord) {
    vec4 ndcPos  = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProjMatrix * ndcPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    // ── 读取深度和法线 ──────────────────────────────────────
    float depth  = texture(depthTex,  vTexCoord).r;
    if (depth >= 1.0) { fragColor = 1.0; return; } // 天空盒，无遮蔽

    vec3 normal  = normalize(texture(normalTex, vTexCoord).rgb * 2.0 - 1.0);
    vec3 fragPos = ReconstructPosition(depth, vTexCoord);

    // ── 旋转噪声（tile 全屏）──────────────────────────────────
    vec2  noiseScale = screenSize / 4.0;
    vec3  randomVec  = normalize(texture(noiseTex, vTexCoord * noiseScale).xyz);

    // ── TBN 矩阵（从法线+随机向量构建切线空间）─────────────────
    vec3 tangent  = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN      = mat3(tangent, bitangent, normal);

    // ── 采样遮蔽 ───────────────────────────────────────────
    float occlusion = 0.0;
    for (int i = 0; i < kernelSize; i++) {
        vec3 samplePos = TBN * ssaoKernel[i]; // 切线→视空间
        samplePos      = fragPos + samplePos * ssaoRadius;

        // 投影到 NDC 采样深度
        vec4 offset     = projMatrix * vec4(samplePos, 1.0);
        offset.xyz     /= offset.w;
        offset.xyz      = offset.xyz * 0.5 + 0.5;

        float sampleDepth = texture(depthTex, offset.xy).r;
        vec3  sampleViewPos = ReconstructPosition(sampleDepth, offset.xy);

        // 范围检查（超出半径的采样不贡献）
        float rangeCheck = smoothstep(0.0, 1.0, ssaoRadius / abs(fragPos.z - sampleViewPos.z + 0.0001));
        occlusion += (sampleViewPos.z >= samplePos.z + ssaoBias ? 1.0 : 0.0) * rangeCheck;
    }
    fragColor = 1.0 - (occlusion / float(kernelSize));
}
