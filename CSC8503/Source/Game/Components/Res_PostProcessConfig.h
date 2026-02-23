#pragma once

struct Res_PostProcessConfig {
    float bloomThreshold  = 1.0f;
    float bloomIntensity  = 0.5f;
    int   bloomIterations = 10;
    float exposure = 1.0f;
    float gamma    = 2.2f;
    bool enableBloom   = true;
    bool enableTonemap = true;
};
