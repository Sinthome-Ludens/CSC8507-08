#version 400 core
// ── FXAA 3.11 Luma-Edge 抗锯齿 ───────────────────────────
// 输入：已经过色调映射的 LDR 图像

in vec2 vTexCoord;

uniform sampler2D screenTex;
uniform vec2      texelSize; // = 1.0 / screenSize

out vec4 fragColor;

#define FXAA_SPAN_MAX    8.0
#define FXAA_REDUCE_MUL  (1.0 / 8.0)
#define FXAA_REDUCE_MIN  (1.0 / 128.0)

float Luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 uv = vTexCoord;
    vec2 ts = texelSize;

    vec3 rgbNW = texture(screenTex, uv + vec2(-1.0, -1.0) * ts).rgb;
    vec3 rgbNE = texture(screenTex, uv + vec2( 1.0, -1.0) * ts).rgb;
    vec3 rgbSW = texture(screenTex, uv + vec2(-1.0,  1.0) * ts).rgb;
    vec3 rgbSE = texture(screenTex, uv + vec2( 1.0,  1.0) * ts).rgb;
    vec3 rgbM  = texture(screenTex, uv).rgb;

    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);
    float lumaM  = Luma(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    // Edge 方向
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (FXAA_REDUCE_MUL * 0.25), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * ts;

    vec3 rgbA = 0.5 * (texture(screenTex, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                        texture(screenTex, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(screenTex, uv + dir * -0.5).rgb +
                                       texture(screenTex, uv + dir *  0.5).rgb);

    float lumaB = Luma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        fragColor = vec4(rgbA, 1.0);
    } else {
        fragColor = vec4(rgbB, 1.0);
    }
}
