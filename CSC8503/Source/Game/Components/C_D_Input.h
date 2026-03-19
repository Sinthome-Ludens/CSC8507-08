/**
 * @file C_D_Input.h
 * @brief 玩家输入数据组件：存储每帧从 Sys_InputDispatch 转换后的输入语义。
 */
#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 玩家输入数据组件
 *
 * 由 Sys_InputDispatch 每帧写入，Sys_Movement / Sys_PlayerDisguise / Sys_PlayerStance /
 * Sys_StealthMetrics / Sys_Item 仅读访问。
 * 将硬件输入转换为游戏语义，解耦输入采集与逻辑消费。
 */
struct C_D_Input {
    // ── 移动向量（世界空间 XZ，已归一化） ──
    float moveX    = 0.0f;   ///< A=-1, D=+1
    float moveZ    = 0.0f;   ///< W=-1, S=+1（-Z 为前方）
    bool  hasInput = false;  ///< |moveVec| > 0.001

    // ── 持续按键 ──
    bool  shiftDown = false;

    // ── 上升沿标志（本帧刚按下） ──
    bool  crouchJustPressed   = false;  ///< C 键
    bool  standJustPressed    = false;  ///< V 键
    bool  disguiseJustPressed = false;  ///< G 键
    bool  cqcJustPressed      = false;  ///< F 键
    bool  gadgetUseJustPressed = false; ///< Q 键 — 使用激活的道具
    bool  weaponUseJustPressed = false; ///< E 键 — 使用激活的武器

    // ── 道具使用上升沿（本帧刚按下，由 Sys_InputDispatch 填入） ──
    bool  item1JustPressed = false; ///< 数字键 1 → 全息诱饵炸弹 (ItemID::HoloBait)
    bool  item2JustPressed = false; ///< 数字键 2 → 光子雷达      (ItemID::PhotonRadar)
    bool  item3JustPressed = false; ///< 数字键 3 → DDoS          (ItemID::DDoS)
    bool  item4JustPressed = false; ///< 数字键 4 → 流窜 AI       (ItemID::RoamAI)
    bool  item5JustPressed = false; ///< 数字键 5 → 靶向打击      (ItemID::TargetStrike)

    // ── 滚轮输入（CQC 目标切换） ──
    int   scrollDelta = 0;
};

} // namespace ECS
