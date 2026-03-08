/**
 * @file Res_VisionConfig.h
 * @brief 敌人视野检测全局配置资源，注册到 Registry context 中供 Sys_EnemyVision 读取。
 *
 * 所有视野相关参数均在此处数据驱动配置，无硬编码常量。
 */
#pragma once

namespace ECS {

/// @brief 敌人视野检测全局配置（数据驱动，注册到 registry ctx）
struct Res_VisionConfig {
    float fovDegrees      = 120.0f;  // 视野全角（度），cosHalfFov = cos(60°) = 0.5
    float maxDistance      = 20.0f;   // 最大检测距离（米）
    float closeRange       = 3.0f;   // 近距离 360° 感知范围（米）
    float visibilityMin    = 0.05f;  // 低于此 visibilityFactor 视为完全隐形
    bool  enableOcclusion  = true;   // 是否启用 CastRay 射线遮挡检测
    float rayOriginHeight  = 1.0f;   // 射线起点 Y 偏移（敌人眼睛高度）
    float rayTargetHeight  = 0.8f;   // 射线终点 Y 偏移（玩家身体中心）
};

} // namespace ECS
