#version 400 core

uniform sampler2D inputTex;
uniform bool      horizontal;

in  vec2 vTexCoord;
out vec4 fragColor;

// 9-tap Gaussian weights
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texelSize = 1.0 / textureSize(inputTex, 0);
    vec3 result = texture(inputTex, vTexCoord).rgb * weight[0];

    if (horizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(inputTex, vTexCoord + vec2(texelSize.x * i, 0.0)).rgb * weight[i];
            result += texture(inputTex, vTexCoord - vec2(texelSize.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(inputTex, vTexCoord + vec2(0.0, texelSize.y * i)).rgb * weight[i];
            result += texture(inputTex, vTexCoord - vec2(0.0, texelSize.y * i)).rgb * weight[i];
        }
    }

    fragColor = vec4(result, 1.0);
}
