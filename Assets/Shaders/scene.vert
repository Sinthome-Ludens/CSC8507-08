#version 430 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec4 tangent; // w = bitangent handedness sign
layout(location = 5) in vec4 jointWeights; // 骨骼权重（4 bones per vertex）
layout(location = 6) in vec4 jointIndices; // 骨骼索引（ivec4，编码为 float）

uniform mat4 modelMatrix  = mat4(1.0);
uniform mat4 viewMatrix   = mat4(1.0);
uniform mat4 projMatrix   = mat4(1.0);

uniform vec4 objectColour     = vec4(1, 1, 1, 1);
uniform bool hasVertexColours = false;

// 骨骼蒙皮
uniform bool useSkinning = false;
uniform mat4 boneMatrices[96];

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

out Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
    mat3 TBN;        // 切线空间→世界空间（供法线贴图）
    vec3 viewNormal; // 视空间法线（供 SSAO G-buffer）
} OUT;

void main() {
    // 选择 per-instance 或 uniform 数据
    mat4 instModel  = useInstancing ? instances[gl_InstanceID].modelMatrix  : modelMatrix;
    vec4 instColour = useInstancing ? instances[gl_InstanceID].objectColour : objectColour;

    // 数据海洋 GPU 噪波路径
    if (useOceanNoise) {
        OceanInstanceData oi = oceanInstances[gl_InstanceID];
        instModel  = oi.modelMatrix;
        instColour = oi.objectColour;
        // GPU 端噪波位移：使用柱子中心 XZ（model matrix 平移列），而非逐顶点世界坐标
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
    vec3 localTan  = tangent.xyz;

    // 骨骼蒙皮变换
    if (useSkinning) {
        ivec4 boneIdx = ivec4(jointIndices);
        mat4 skinMat  = boneMatrices[boneIdx.x] * jointWeights.x
                      + boneMatrices[boneIdx.y] * jointWeights.y
                      + boneMatrices[boneIdx.z] * jointWeights.z
                      + boneMatrices[boneIdx.w] * jointWeights.w;
        localPos  = skinMat * localPos;
        localNorm = mat3(skinMat) * localNorm;
        localTan  = mat3(skinMat) * localTan;
    }

    mat3 normalMatrix = transpose(inverse(mat3(instModel)));

    OUT.worldPos = (instModel * localPos).xyz;

    vec3 N = normalize(normalMatrix * normalize(localNorm));
    vec3 T = normalize(normalMatrix * localTan);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt 正交化
    vec3 B = cross(N, T) * tangent.w;  // bitangent sign 来自 w 分量

    OUT.normal     = N;
    OUT.TBN        = mat3(T, B, N);
    OUT.viewNormal = normalize(mat3(viewMatrix * instModel) * normalize(localNorm));

    OUT.texCoord = texCoord;
    OUT.colour   = instColour;
    if (hasVertexColours) {
        OUT.colour = instColour * colour;
    }

    gl_Position = projMatrix * viewMatrix * vec4(OUT.worldPos, 1.0);
}
