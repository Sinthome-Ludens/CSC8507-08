#version 430 core

// ── shadow pass 顶点着色器（支持 Normal Offset Bias + 骨骼蒙皮 + SSBO Instancing）──

// 旧接口保留：C++ 端仍可用 mvpMatrix（单级联兼容）
uniform mat4 mvpMatrix     = mat4(1.0);
// 新接口：分解 M/V/P 以便计算 Normal Offset
uniform mat4 modelMatrix   = mat4(1.0);
uniform mat4 lightVPMatrix = mat4(1.0);  // lightProj * lightView
uniform bool useModelMatrix = false;     // true=使用 modelMatrix + lightVPMatrix

// Normal Offset Bias（沿世界空间法线偏移，消除 shadow acne）
// texelSize = lightFrustumWidth / shadowMapResolution
uniform float normalOffsetBias = 0.0;

// 骨骼蒙皮
uniform bool  useSkinning = false;
uniform mat4  boneMatrices[96];

// Instanced rendering SSBO
uniform bool useInstancing = false;

struct InstanceData {
    mat4 modelMatrix;
    vec4 objectColour;
};

layout(std430, binding = 0) buffer InstanceBuffer {
    InstanceData instances[];
};

// ── 数据海洋 GPU 噪波 ──────────────────────────────────
uniform bool  useOceanNoise      = false;
uniform float oceanTime          = 0.0;
uniform float oceanNoiseScale    = 0.05;
uniform float oceanNoiseSpeed    = 0.3;
uniform float oceanBaseAmplitude = 3.0;

struct OceanInstanceData {
    mat4 modelMatrix;
    vec4 objectColour;
    vec4 pillarParams;  // x=baseY, y=amplitude, z=phaseShift, w=unused
};

layout(std430, binding = 1) buffer OceanInstanceBuffer {
    OceanInstanceData oceanInstances[];
};

// ── GPU 噪波函数（与 NoiseUtil.h 一一对应）──────────────
float oceanHashFloat(int x, int y, int z) {
    int n = x * 73856093 ^ y * 19349663 ^ z * 83492791;
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return float(n & 0x7fffffff) / 1073741824.0 - 1.0;
}

float oceanFade(float t) {
    return t * t * (3.0 - 2.0 * t);
}

float oceanNoise3D(float x, float y, float z) {
    int ix = int(floor(x)); int iy = int(floor(y)); int iz = int(floor(z));
    float fx = x - float(ix); float fy = y - float(iy); float fz = z - float(iz);
    float u = oceanFade(fx); float v = oceanFade(fy); float w = oceanFade(fz);

    float c000 = oceanHashFloat(ix,     iy,     iz);
    float c100 = oceanHashFloat(ix + 1, iy,     iz);
    float c010 = oceanHashFloat(ix,     iy + 1, iz);
    float c110 = oceanHashFloat(ix + 1, iy + 1, iz);
    float c001 = oceanHashFloat(ix,     iy,     iz + 1);
    float c101 = oceanHashFloat(ix + 1, iy,     iz + 1);
    float c011 = oceanHashFloat(ix,     iy + 1, iz + 1);
    float c111 = oceanHashFloat(ix + 1, iy + 1, iz + 1);

    float x00 = mix(c000, c100, u);
    float x10 = mix(c010, c110, u);
    float x01 = mix(c001, c101, u);
    float x11 = mix(c011, c111, u);

    float y0 = mix(x00, x10, v);
    float y1 = mix(x01, x11, v);

    return mix(y0, y1, w);
}

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec4 tangent;
layout(location = 5) in vec4 jointWeights;
layout(location = 6) in vec4 jointIndices;

out vec2 vTexCoord;

void main() {
    mat4 instModel = useInstancing ? instances[gl_InstanceID].modelMatrix : modelMatrix;

    // 数据海洋 GPU 噪波路径
    if (useOceanNoise) {
        OceanInstanceData oi = oceanInstances[gl_InstanceID];
        instModel = oi.modelMatrix;
        // 使用柱子中心 XZ（model matrix 平移列），而非逐顶点世界坐标
        vec3 center = instModel[3].xyz;
        float n = oceanNoise3D(
            center.x * oceanNoiseScale,
            center.z * oceanNoiseScale,
            oceanTime * oceanNoiseSpeed + oi.pillarParams.z
        );
        float newY = oi.pillarParams.x + n * oi.pillarParams.y * oceanBaseAmplitude;
        instModel[3].y = newY;
    }

    vec4 localPos  = vec4(position, 1.0);
    vec3 localNorm = normal;

    if (useSkinning) {
        ivec4 boneIdx = ivec4(jointIndices);
        mat4 skinMat  = boneMatrices[boneIdx.x] * jointWeights.x
                      + boneMatrices[boneIdx.y] * jointWeights.y
                      + boneMatrices[boneIdx.z] * jointWeights.z
                      + boneMatrices[boneIdx.w] * jointWeights.w;
        localPos  = skinMat * localPos;
        localNorm = mat3(skinMat) * localNorm;
    }

    vTexCoord = texCoord;

    if (useModelMatrix || useInstancing || useOceanNoise) {
        mat3 normalMatrix = transpose(inverse(mat3(instModel)));
        vec3 worldNormal  = normalize(normalMatrix * normalize(localNorm));
        vec3 worldPos     = (instModel * localPos).xyz;
        worldPos          += worldNormal * normalOffsetBias;
        gl_Position       = lightVPMatrix * vec4(worldPos, 1.0);
    } else {
        gl_Position = mvpMatrix * localPos;
    }
}
