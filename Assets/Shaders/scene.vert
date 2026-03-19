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
