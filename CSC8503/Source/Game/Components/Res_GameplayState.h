#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 敌人警戒状态枚举（策划文档 §2.1.B 敌人状态机）
 *
 * 由 alertLevel 阈值驱动，HUD 根据此状态显示文字与配色。
 */
enum class AlertStatus : uint8_t {
    Safe   = 0,   // 0-15: 巡逻中，无威胁
    Search = 1,   // 16-30: 发现痕迹，搜索中
    Alert  = 2,   // 31-50: 完全发现，前往最后已知位置
    Hunt   = 3,   // 51-100: 主动追击
    Raid   = 4,   // 101-150: 全面搜捕（特殊搜捕模式）
};

/**
 * @brief 玩家移动状态枚举（策划文档 §2.1.A 玩家状态机）
 */
enum class PlayerMoveState : uint8_t {
    Standing = 0,  // 站立（2档速度 + 2档可见度）
    Crouching = 1, // 下蹲（1档速度 + 1档可见度）
    Running  = 2,  // 奔跑（3档速度 + 最高可见度）
};

/**
 * @brief 游戏玩法全局状态资源
 *
 * 存储所有 HUD 需要读取的实时游戏状态数据。
 * 由各游戏系统写入（Sys_AI 写警戒度、Sys_PlayerController 写玩家状态），
 * 由 Sys_UI 读取用于 HUD 渲染。
 *
 * 当前 Phase 2：各系统尚未实现，使用模拟数据驱动 HUD 开发。
 *
 * @note 作为 Registry Context 资源（非 per-entity Component），
 *       不受 64 字节限制，但仍保持 POD 特性。
 *
 * @see Sys_UI::RenderHUD (读取方)
 * @see 策划文档 §2.1 玩家系统、§2.1.B 敌人系统、§2.2 道具系统
 */
struct Res_GameplayState {
    // ── 警戒度系统（策划文档 §2.1.B）──
    // 全局浮点数，最高上限 150.0。
    // 上涨: (基础上涨值 × 视野区域补正 × 玩家状态补正) × 难度倍率
    // 不会自然降低，仅与道具/特殊事件相关。
    float alertLevel      = 0.0f;    // 当前警戒度 [0.0, 150.0]
    float alertMax        = 150.0f;  // 最高上限

    // ── 倒计时系统（策划文档 §2.1.C）──
    // alertLevel >= 1.0 时激活，一旦启动不可暂停/重置/增加。
    // countdownTimer 递减到 0 → 游戏失败。
    float countdownTimer  = 0.0f;    // 剩余秒数
    float countdownMax    = 120.0f;  // 倒计时总时长（秒）
    bool  countdownActive = false;   // 是否已启动

    // ── 玩家状态（策划文档 §2.1.A）──
    PlayerMoveState playerMoveState = PlayerMoveState::Standing;
    bool  playerDisguised = false;   // 拟态伪装中（换装后同类敌人无法索敌）

    // ── 任务信息 ──
    char missionName[32]   = {};     // 当前行动名称（左上显示）
    char objectiveText[48] = {};     // 当前目标描述（左上显示）

    // ── 道具/武器快捷栏（策划文档 §2.2）──
    // 实际道具数据由 Sys_Inventory 管理，HUD 仅显示。
    // Phase 2 使用模拟数据。
    uint8_t activeItemSlot   = 0;    // 当前选中道具槽 [0, 1]
    uint8_t activeWeaponSlot = 0;    // 当前选中武器槽 [0, 1]

    // 道具显示信息（由 Sys_Inventory 每帧写入，Sys_UI 读取）
    struct SlotDisplay {
        char name[16] = {};          // 道具/武器名称
        int8_t count  = 0;           // 剩余数量（-1 = 无限）
        float cooldown = 0.0f;       // 冷却进度 [0.0, 1.0]，0 = 可用
    };
    SlotDisplay itemSlots[2]   = {}; // 2个道具槽
    SlotDisplay weaponSlots[2] = {}; // 2个武器槽
};

/**
 * @brief 根据警戒度值计算当前警戒状态
 * @param alertLevel 当前警戒度 [0.0, 150.0]
 * @return 对应的 AlertStatus 枚举值
 */
inline AlertStatus GetAlertStatus(float alertLevel) {
    if (alertLevel <= 15.0f)  return AlertStatus::Safe;
    if (alertLevel <= 30.0f)  return AlertStatus::Search;
    if (alertLevel <= 50.0f)  return AlertStatus::Alert;
    if (alertLevel <= 100.0f) return AlertStatus::Hunt;
    return AlertStatus::Raid;
}

/**
 * @brief 获取警戒状态的显示文字
 */
inline const char* GetAlertStatusText(AlertStatus status) {
    switch (status) {
        case AlertStatus::Safe:   return "SAFE";
        case AlertStatus::Search: return "SEARCH";
        case AlertStatus::Alert:  return "ALERT";
        case AlertStatus::Hunt:   return "HUNT";
        case AlertStatus::Raid:   return "RAID";
        default:                  return "UNKNOWN";
    }
}

} // namespace ECS
