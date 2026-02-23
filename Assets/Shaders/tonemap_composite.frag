#version 400 core

uniform sampler2D hdrTex;
uniform sampler2D bloomTex;
uniform float     exposure;
uniform float     gamma;
uniform float     bloomIntensity;
uniform bool      enableBloom;
uniform bool      enableTonemap;

in  vec2 vTexCoord;
out vec4 fragColor;

// ACES Filmic Tone Mapping
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture(hdrTex, vTexCoord).rgb;

    // Bloom composite
    if (enableBloom) {
        vec3 bloom = texture(bloomTex, vTexCoord).rgb;
        hdrColor += bloom * bloomIntensity;
    }

    vec3 result = hdrColor;

    // Tone mapping
    if (enableTonemap) {
        result = ACESFilm(result * exposure);
    }

    // Gamma correction
    result = pow(result, vec3(1.0 / gamma));

    fragColor = vec4(result, 1.0);
}
