/**
 * @file Res_ScoreConfig.h
 * @brief 战役积分系统配置资源：集中管理所有积分惩罚值、评级阈值和初始分数。
 *
 * 消除 Sys_Countdown / Sys_DeathJudgment / Sys_PlayerCQC / Sys_ItemEffects /
 * Sys_LevelGoal / Main.cpp 中分散的积分 magic number，实现数据驱动的积分平衡。
 *
 * 注册方式：ctx_emplace<Res_ScoreConfig>()（场景 OnAwake 或 Main.cpp 初始化时）
 */
#pragma once

#include <cstdint>

namespace ECS {

struct Res_ScoreConfig {
    // ── 初始积分 ──
    int32_t initialScore              = 1000;

    // ── 事件惩罚值 ──
    int32_t penaltyCountdownSurge     = 200;    ///< 倒计时触发（alertLevel 满）
    int32_t penaltyCountdownExpire    = 500;    ///< 倒计时耗尽（任务失败）
    int32_t penaltyCapture            = 500;    ///< 被敌人发现/抓捕
    int32_t penaltyKill               = 10;     ///< 击杀敌人（CQC / 武器 / 道具）
    int32_t penaltyItemUse            = 5;      ///< 使用道具

    // ── 及格线 ──
    int32_t passThreshold             = 500;    ///< 积分 > 此值 = 成功，<= 此值 = 失败

    // ── 评级阈值（从低到高：F/D/C/B/A/S/SS/SSS）──
    // score <= thresholds[i] 对应 tier i，score > thresholds[6] 对应 tier 7 (SSS)
    static constexpr int RATING_COUNT = 8;
    int32_t ratingThresholds[7]       = { 500, 599, 699, 799, 899, 949, 969 };
};

/// @brief 评级名称数组（按 tier 索引：0=F, 1=D, ..., 7=SSS）。
inline constexpr const char* const kScoreRatingNames[] = {"F","D","C","B","A","S","SS","SSS"};

/// @brief 根据积分和配置返回评级字符串（F/D/C/B/A/S/SS/SSS）。
inline const char* GetScoreRating(int32_t score, const Res_ScoreConfig& cfg) {
    for (int i = 0; i < 7; ++i) {
        if (score <= cfg.ratingThresholds[i]) return kScoreRatingNames[i];
    }
    return kScoreRatingNames[7];
}

/// @brief 返回评级数值档位（0=F, 1=D, ..., 7=SSS），用于降级检测。
inline int8_t GetScoreRatingTier(int32_t score, const Res_ScoreConfig& cfg) {
    for (int i = 0; i < 7; ++i) {
        if (score <= cfg.ratingThresholds[i]) return static_cast<int8_t>(i);
    }
    return 7;
}

} // namespace ECS
