#version 400 core

uniform mat4 modelMatrix  = mat4(1.0);
uniform mat4 viewMatrix   = mat4(1.0);
uniform mat4 projMatrix   = mat4(1.0);

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec4 tangent; // w = bitangent handedness sign

uniform vec4 objectColour     = vec4(1, 1, 1, 1);
uniform bool hasVertexColours = false;

out Vertex {
    vec4 colour;
    vec2 texCoord;
    vec3 normal;
    vec3 worldPos;
    mat3 TBN;        // 切线空间→世界空间（供法线贴图）
    vec3 viewNormal; // 视空间法线（供 SSAO G-buffer）
} OUT;

void main() {
    mat4 mvp          = projMatrix * viewMatrix * modelMatrix;
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));

    OUT.worldPos = (modelMatrix * vec4(position, 1.0)).xyz;

    vec3 N = normalize(normalMatrix * normalize(normal));
    vec3 T = normalize(normalMatrix * tangent.xyz);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt 正交化
    vec3 B = cross(N, T) * tangent.w;  // bitangent sign 来自 w 分量

    OUT.normal     = N;
    OUT.TBN        = mat3(T, B, N);
    OUT.viewNormal = normalize(mat3(viewMatrix * modelMatrix) * normalize(normal));

    OUT.texCoord = texCoord;
    OUT.colour   = objectColour;
    if (hasVertexColours) {
        OUT.colour = objectColour * colour;
    }

    gl_Position = mvp * vec4(position, 1.0);
}
