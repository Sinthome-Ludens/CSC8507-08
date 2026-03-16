#version 400 core
// ── IBL 辐照度卷积（漫反射 IBL）────────────────────────────
// 对输入立方体贴图进行半球卷积，生成 32×32 辐照度贴图
// 用法：对目标立方体贴图的每个面各调用一次，通过 face uniform 指定面索引

in vec2 vTexCoord; // [0,1]

uniform samplerCube environmentMap; // 输入：skybox 立方体贴图
uniform int         face;           // 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z

out vec4 fragColor;

const float PI = 3.14159265359;

/// @brief 从面索引和 UV 计算方向向量
vec3 FaceUVtoDir(int f, vec2 uv) {
    vec2 p = uv * 2.0 - 1.0;
    if      (f == 0) return normalize(vec3( 1.0, -p.y, -p.x));
    else if (f == 1) return normalize(vec3(-1.0, -p.y,  p.x));
    else if (f == 2) return normalize(vec3( p.x,  1.0,  p.y));
    else if (f == 3) return normalize(vec3( p.x, -1.0, -p.y));
    else if (f == 4) return normalize(vec3( p.x, -p.y,  1.0));
    else             return normalize(vec3(-p.x, -p.y, -1.0));
}

void main() {
    vec3 N = FaceUVtoDir(face, vTexCoord);
    // 构建切线空间基（N 为法线）
    vec3 up    = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = cross(N, right);

    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.025;
    float nrSamples   = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // 球坐标→切线空间→世界空间
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec     = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance / nrSamples;
    fragColor  = vec4(irradiance, 1.0);
}
