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
 * - **Sys_UI**：读取所有字段显示 HUD
 *
 * @note 多个 System 可能同时写入此资源，需注意逻辑顺序。
 */

#pragma once

#include <cstdint>

namespace ECS {

// ── 警戒等级枚举 ──────────────────────────────────────────────
enum class AlertStatus : uint8_t {
    Safe   = 0,   // 0 ~ 25
    Search = 1,   // 26 ~ 50
    Alert  = 2,   // 51 ~ 75
    Hunt   = 3,   // 76 ~ 100
};

/**
 * @brief 根据警戒等级数值返回对应的 AlertStatus 枚举。
 * @param alertLevel 当前警戒等级（0.0 ~ 100.0）
 * @return 对应的 AlertStatus 值（Safe/Search/Alert/Hunt）
 */
inline AlertStatus GetAlertStatus(float alertLevel) {
    if (alertLevel <= 25.0f)  return AlertStatus::Safe;
    if (alertLevel <= 50.0f)  return AlertStatus::Search;
    if (alertLevel <= 75.0f)  return AlertStatus::Alert;
    return AlertStatus::Hunt;
}

/**
 * @brief 将 AlertStatus 枚举转换为可显示的字符串。
 * @param s AlertStatus 枚举值
 * @return 对应的英文大写字符串（"SAFE"/"SEARCH"/"ALERT"/"HUNT"/"UNKNOWN"）
 */
inline const char* GetAlertStatusText(AlertStatus s) {
    switch (s) {
        case AlertStatus::Safe:   return "SAFE";
        case AlertStatus::Search: return "SEARCH";
        case AlertStatus::Alert:  return "ALERT";
        case AlertStatus::Hunt:   return "HUNT";
        default:                  return "UNKNOWN";
    }
}

/// @brief 玩家移动状态枚举（Standing/Crouching/Running），由 Sys_Input 写入。
enum class PlayerMoveState : uint8_t {
    Standing  = 0,
    Crouching = 1,
    Running   = 2,
};

/// @brief 多人比赛阶段（等待/开始/进行中/结束）。
enum class MatchPhase : uint8_t {
    WaitingForPeer = 0,
    Starting       = 1,
    Running        = 2,
    Finished       = 3,
};

/// @brief 多人比赛结果（供 GameOver/UI 读取）。
enum class MatchResult : uint8_t {
    None         = 0,
    LocalWin     = 1,
    OpponentWin  = 2,
    Draw         = 3,
    Disconnected = 4,
};

/// @brief 多人比赛固定为三关，用于三段式进度条和比赛结束判定。
static constexpr uint8_t kMultiplayerStageCount = 3;

/// @brief 装备槽显示数据（名称/数量/冷却进度），由 HUD 渲染直接读取。
struct SlotDisplay {
    char    name[24] = {};
    uint8_t itemId   = 0;      ///< 对应 ItemID 枚举值，供击杀通知等使用
    uint8_t count    = 0;
    float   cooldown = 0.0f;   ///< 0.0 = ready，>0 = 冷却中（归一化 0~1）
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
    float countdownTimer   = 30.0f;
    float countdownMax     = 30.0f;
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

    // ─ 道具使用闪光 ───────────────────────────────────────
    float   itemUseFlashTimer    = 0.0f;   ///< >0 时 HUD 面板闪光
    uint8_t itemUseFlashSlotType = 0;      ///< 0=gadget, 1=weapon

    // ─ 噪音 ───────────────────────────────────────────────
    float noiseLevel = 0.0f;  ///< [0.0, 1.0]

    // ─ GameOver ───────────────────────────────────────────
    uint8_t gameOverReason = 0;   ///< 0=无, 1=倒计时, 2=被发现, 3=成功
    float   gameOverTime   = 0.0f;

    // ─ 累计游玩时间 ──────────────────────────────────────
    float playTime = 0.0f;

    // ─ 多人对战 (1v1) ──────────────────────────────────────
    bool       isMultiplayer         = false;  ///< 当前是否为多人模式
    MatchPhase matchPhase            = MatchPhase::WaitingForPeer; ///< 当前比赛阶段
    MatchResult matchResult          = MatchResult::None;          ///< 当前比赛结果
    uint8_t    currentRoundIndex     = 0;      ///< 当前处于第几关 [0,2]
    uint8_t    localStageProgress    = 0;      ///< 本地已完成关卡数 [0,3]
    uint8_t    opponentStageProgress = 0;      ///< 对手已完成关卡数 [0,3]
    bool       roundJustAdvanced     = false;  ///< 本帧是否刚推进一关
    bool       matchJustStarted      = false;  ///< 本帧是否刚开始比赛
    bool       matchJustFinished     = false;  ///< 本帧是否刚结束比赛
    char       opponentName[16]      = "RIVAL";///< 对手显示名称
    uint8_t    disruptionType        = 0;      ///< 受到的干扰类型: 0=无 1=视觉干扰 2=减速 3=信号扰乱
    float      disruptionTimer       = 0.0f;   ///< 干扰剩余时长
    float      disruptionDuration    = 0.0f;   ///< 干扰总时长
    uint32_t   networkPing           = 0;      ///< 网络延迟 RTT (ms)

    // 兼容旧 HUD 逻辑：Phase 0 仅新增三阶段状态，不在本阶段移除旧百分比字段。
    uint8_t  localProgress       = 0;      ///< Deprecated: 旧多人 HUD 进度 (0-100%)
    uint8_t  opponentProgress    = 0;      ///< Deprecated: 旧多人 HUD 进度 (0-100%)

};

} // namespace ECS
