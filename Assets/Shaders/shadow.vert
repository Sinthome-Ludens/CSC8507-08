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

    if (useModelMatrix || useInstancing) {
        mat3 normalMatrix = transpose(inverse(mat3(instModel)));
        vec3 worldNormal  = normalize(normalMatrix * normalize(localNorm));
        vec3 worldPos     = (instModel * localPos).xyz;
        worldPos          += worldNormal * normalOffsetBias;
        gl_Position       = lightVPMatrix * vec4(worldPos, 1.0);
    } else {
        gl_Position = mvpMatrix * localPos;
    }
}
