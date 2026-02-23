#version 400 core

uniform sampler2D hdrTex;
uniform float     threshold;

in  vec2 vTexCoord;
out vec4 fragColor;

void main() {
    vec3 color = texture(hdrTex, vTexCoord).rgb;

    // Soft knee threshold extraction
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float soft = brightness - threshold + 0.5;
    soft = clamp(soft, 0.0, 1.0);
    soft = soft * soft;

    float contribution = max(soft, step(threshold, brightness));
    fragColor = vec4(color * contribution, 1.0);
}
