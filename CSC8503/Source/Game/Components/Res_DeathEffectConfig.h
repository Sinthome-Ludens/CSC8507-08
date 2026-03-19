/**
 * @file Res_DeathEffectConfig.h
 * @brief 死亡特效动画配置资源：四阶段数字溶解效果的全部可调参数。
 *
 * 消除 Sys_DeathEffect.cpp 中 55+ 个内联 magic number，
 * 支持运行时调参（可通过 ImGui 面板实时修改）。
 */
#pragma once

namespace ECS {

struct Res_DeathEffectConfig {
    // ── Phase 时间边界（秒）──
    float phase1End   = 0.15f;   ///< 数字冲击结束
    float phase2End   = 0.55f;   ///< 霓虹故障结束
    float phase3End   = 1.00f;   ///< 数据溶解结束
    float phase4End   = 1.20f;   ///< 最终崩塌结束 = 动画总时长

    // ── Phase 1: 数字冲击 ──
    float p1_emissiveStrength  = 4.0f;
    float p1_rimPower          = 2.0f;
    float p1_rimStrength       = 1.5f;

    // ── Phase 2: 霓虹故障 ──
    float p2_colorCycleHz      = 12.5f;  ///< 三色循环频率
    float p2_pulseMid          = 2.25f;  ///< emissive 脉冲中值
    float p2_pulseAmp          = 0.75f;  ///< emissive 脉冲振幅
    float p2_pulseHz           = 25.0f;  ///< sin 脉冲频率
    float p2_flickerHz         = 80.0f;  ///< alpha 闪烁采样率
    float p2_flickerOff        = 0.3f;   ///< 闪烁暗态 alpha
    float p2_flickerOn         = 1.0f;   ///< 闪烁亮态 alpha
    float p2_jitterHz          = 60.0f;  ///< 轴抖动采样率
    float p2_jitterAmp         = 0.10f;  ///< 轴抖动幅度 (±5%)
    float p2_rimPower          = 3.0f;
    float p2_rimStrength       = 0.8f;

    // ── Phase 3: 数据溶解 ──
    float p3_emissiveStart     = 2.0f;
    float p3_emissiveDecay     = 1.7f;   ///< emissive 衰减量
    float p3_alphaStart        = 1.0f;
    float p3_alphaDecay        = 0.85f;
    float p3_stutterHz         = 20.0f;
    int   p3_stutterModulo     = 8;      ///< 1/N 概率触发 stutter
    float p3_stutterAlpha      = 0.6f;
    float p3_yStretch          = 0.2f;   ///< Y 轴拉伸量
    float p3_xzShrink          = 0.4f;   ///< XZ 轴收缩量
    float p3_rimPowerStart     = 3.0f;
    float p3_rimPowerGrow      = 2.0f;
    float p3_rimStrengthStart  = 0.8f;
    float p3_rimStrengthDecay  = 0.6f;

    // ── Phase 4: 最终崩塌 ──
    float p4_emissiveStart     = 1.0f;
    float p4_alphaStart        = 0.15f;
    float p4_scaleCollapse     = 0.9f;   ///< 收缩量（1.0 - 此值 = 最终比例）
    float p4_rimPower          = 5.0f;
    float p4_rimStrength       = 0.0f;

    // Phase 3 终态（Phase 4 起始继承）
    float p3EndXZScale         = 0.6f;   ///< = 1.0 - p3_xzShrink
    float p3EndYScale          = 1.2f;   ///< = 1.0 + p3_yStretch
};

} // namespace ECS
