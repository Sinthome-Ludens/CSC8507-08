#version 400 core

// ── 顶点属性 ──────────────────────────────────────────────
layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec4 tangent;      // w = bitangent sign (handedness)
layout(location = 5) in vec4 jointWeights; // 骨骼权重（4 bones per vertex）
layout(location = 6) in vec4 jointIndices; // 骨骼索引（ivec4，编码为 float）

// ── 变换矩阵 ──────────────────────────────────────────────
uniform mat4 modelMatrix  = mat4(1.0);
uniform mat4 viewMatrix   = mat4(1.0);
uniform mat4 projMatrix   = mat4(1.0);
uniform vec4 objectColour = vec4(1, 1, 1, 1);
uniform bool hasVertexColours = false;

// ── 骨骼蒙皮 ──────────────────────────────────────────────
uniform bool useSkinning = false;
uniform mat4 boneMatrices[96];

// ── 顶点输出接口块 ─────────────────────────────────────────
out Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 worldPos;
    vec3 normal;    // 世界空间法线
    mat3 TBN;       // 切线空间→世界空间变换矩阵
    vec3 viewNormal; // 视空间法线（供 SSAO G-buffer 使用）
} OUT;

void main() {
    vec4 localPos  = vec4(position, 1.0);
    vec3 localNorm = normal;
    vec3 localTan  = tangent.xyz;

    // ── 骨骼蒙皮变换 ────────────────────────────────────────
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

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    // ── 世界空间输出 ────────────────────────────────────────
    OUT.worldPos  = (modelMatrix * localPos).xyz;

    vec3 N = normalize(normalMatrix * normalize(localNorm));
    vec3 T = normalize(normalMatrix * localTan);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt 正交化
    vec3 B = cross(N, T) * tangent.w;  // bitangent sign 来自 w 分量

    OUT.normal  = N;
    OUT.TBN     = mat3(T, B, N);

    // 视空间法线（供 SSAO normalTex 写入）
    OUT.viewNormal = normalize(mat3(viewMatrix * modelMatrix) * normalize(localNorm));

    OUT.texCoord = texCoord;
    OUT.colour   = objectColour;
    if (hasVertexColours) {
        OUT.colour = objectColour * colour;
    }

    gl_Position = projMatrix * viewMatrix * vec4(OUT.worldPos, 1.0);
}
