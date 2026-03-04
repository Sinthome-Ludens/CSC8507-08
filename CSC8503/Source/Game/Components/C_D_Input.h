#pragma once

namespace ECS {

/**
 * @brief 玩家输入数据组件
 *
 * 由 Sys_InputDispatch 每帧写入，Sys_Gameplay / Sys_Movement 只读访问。
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
    bool  disguiseJustPressed = false;  ///< E 键
    bool  cqcJustPressed      = false;  ///< F 键
};

} // namespace ECS
