#pragma once
#include <cstdint>

namespace ECS {

/**
 * @brief 玩家输入位掩码 (Bitmask)
 * 
 * 使用位运算存储玩家的输入意图，支持最多32个不同的按键/动作。
 * 方便网络传输并遵循开闭原则，新增动作只需在此枚举中增加对应位即可。
 */
enum PlayerInputFlags : uint32_t {
    None    = 0,
    Up      = 1 << 0,
    Down    = 1 << 1,
    Left    = 1 << 2,
    Right   = 1 << 3,
    Jump    = 1 << 4,
    Fire    = 1 << 5
    // 未来可以在这里继续追加，如 Crouch = 1 << 6, Interact = 1 << 7 等
};

/**
 * @brief 玩家输入数据组件
 * 
 * 存储玩家控制实体（本地或网络同步）的移动/行为输入状态。
 * 由输入/网络系统写入，由物理系统或其他逻辑系统读取。
 */
struct C_D_PlayerInput {
    uint32_t buttonMask = 0; ///< 当前帧的按键状态掩码
};

} // namespace ECS
