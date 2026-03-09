/**
 * @file Res_GameState.h
 * @brief 全局游戏状态资源：警戒/倒计时/玩家状态/装备槽/噪音/GameOver 等
 *
 * @details
 * `Res_GameState` 存储当前游戏会话的全局状态，由多个 System 协作维护。
 *
 * ## 维护责任
 *
 * - **Sys_Combat**：修改 `score`、`playerLives`
 * - **Sys_EnemySpawner**：修改 `enemyCount`
 * - **Sys_Alert**：修改 `alertLevel`
 * - **Sys_Countdown**：修改 `countdownTimer`、`countdownActive`、`gameOverReason`
 * - **Sys_Chat**：读写 `alertLevel`（回复效果）
 * - **Sys_PlayerCQC**：写入 `killNotifyActive/Timer`（CQC 击杀触发）
 * - **Sys_DeathJudgment**：写入 `killNotifyActive/Timer`（HP 归零触发）
 * - **Sys_UI**：读取所有字段显示 HUD；推进 `killNotifyTimer`，到期清除 `killNotifyActive`
 *
 * @note 多个 System 可能同时写入此资源，需注意逻辑顺序。
 */

#pragma once

#include <cstdint>

namespace ECS {

// ── 警戒等级枚举 ──────────────────────────────────────────────
enum class AlertStatus : uint8_t {
    Safe   = 0,   // 0 ~ 15
    Search = 1,   // 16 ~ 30
    Alert  = 2,   // 31 ~ 50
    Hunt   = 3,   // 51 ~ 100
};

inline AlertStatus GetAlertStatus(float alertLevel) {
    if (alertLevel <= 15.0f)  return AlertStatus::Safe;
    if (alertLevel <= 30.0f)  return AlertStatus::Search;
    if (alertLevel <= 50.0f)  return AlertStatus::Alert;
    return AlertStatus::Hunt;
}

inline const char* GetAlertStatusText(AlertStatus s) {
    switch (s) {
        case AlertStatus::Safe:   return "SAFE";
        case AlertStatus::Search: return "SEARCH";
        case AlertStatus::Alert:  return "ALERT";
        case AlertStatus::Hunt:   return "HUNT";
        default:                  return "UNKNOWN";
    }
}

// ── 玩家移动状态 ──────────────────────────────────────────────
enum class PlayerMoveState : uint8_t {
    Standing  = 0,
    Crouching = 1,
    Running   = 2,
};

// ── 装备槽显示数据 ──────────────────────────────────────────
struct SlotDisplay {
    char    name[16] = {};
    uint8_t count    = 0;
    float   cooldown = 0.0f;   // 0.0 = ready, >0 = on cooldown (normalized 0~1)
};

/**
 * @brief 全局游戏状态资源
 */
struct Res_GameState {
    // ─ 原有字段（保持不变）────────────────────────────────
    uint32_t score        = 0;
    uint32_t currentLevel = 1;
    uint32_t playerLives  = 3;

    uint32_t enemyCount   = 0;
    float    alertLevel   = 0.0f;   ///< 全局警戒等级（0.0 ~ 100.0）
    float    alertMax     = 100.0f; ///< 警戒等级上限

    bool isPaused   = false;
    bool isGameOver = false;

    // ─ 倒计时 ─────────────────────────────────────────────
    float countdownTimer   = 32.0f;
    float countdownMax     = 32.0f;
    bool  countdownActive  = false;

    // ─ 玩家状态 ───────────────────────────────────────────
    PlayerMoveState playerMoveState = PlayerMoveState::Standing;
    bool playerDisguised = false;

    // ─ 任务信息 ───────────────────────────────────────────
    char missionName[32]   = "OPERATION";
    char objectiveText[48] = "INFILTRATE TARGET";

    // ─ 装备槽 ─────────────────────────────────────────────
    uint8_t     activeItemSlot   = 0;   ///< 活跃道具槽 [0,1]
    uint8_t     activeWeaponSlot = 0;   ///< 活跃武器槽 [0,1]
    SlotDisplay itemSlots[2]     = {};
    SlotDisplay weaponSlots[2]   = {};

    // ─ 噪音 ───────────────────────────────────────────────
    float noiseLevel = 0.0f;  ///< [0.0, 1.0]

    // ─ GameOver ───────────────────────────────────────────
    uint8_t gameOverReason = 0;   ///< 0=无, 1=倒计时, 2=被发现, 3=成功
    float   gameOverTime   = 0.0f;

    // ─ 累计游玩时间 ──────────────────────────────────────
    float playTime = 0.0f;

    // ─ 多人对战 (1v1) ──────────────────────────────────────
    bool     isMultiplayer       = false;  ///< 当前是否为多人模式
    uint8_t  localProgress       = 0;      ///< 本地玩家目标进度 (0-100%)
    uint8_t  opponentProgress    = 0;      ///< 对手目标进度 (0-100%)
    char     opponentName[16]    = "RIVAL";///< 对手显示名称
    uint8_t  disruptionType      = 0;      ///< 受到的干扰类型: 0=无 1=视觉干扰 2=减速 3=信号扰乱
    float    disruptionTimer     = 0.0f;   ///< 干扰剩余时长
    float    disruptionDuration  = 0.0f;   ///< 干扰总时长
    uint32_t networkPing         = 0;      ///< 网络延迟 RTT (ms)

    // ─ 击杀通知 ─────────────────────────────────────────
    bool  killNotifyActive   = false;   ///< 是否正在显示击杀通知
    float killNotifyTimer    = 0.0f;    ///< 已经过时间（秒）
    float killNotifyDuration = 2.0f;    ///< 总显示时长（秒）
};

} // namespace ECS
