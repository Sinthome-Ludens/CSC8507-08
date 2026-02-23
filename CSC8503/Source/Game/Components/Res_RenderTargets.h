#pragma once

#include "glad/gl.h"

/// @brief 每帧由 Main.cpp 更新的渲染目标资源
/// 解耦 ECS 系统与 GameTechRenderer，Sys_PostProcess 只需读取此组件
struct Res_RenderTargets {
    GLuint hdrColorTex = 0;
    int    width       = 0;
    int    height      = 0;
};
