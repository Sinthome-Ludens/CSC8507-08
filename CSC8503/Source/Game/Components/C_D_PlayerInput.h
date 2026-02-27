#pragma once

namespace ECS {

/**
 * @brief 玩家输入数据组件
 * 
 * 存储玩家控制实体（本地或网络同步）的移动输入状态。
 * 由输入/网络系统写入，由物理系统读取并应用对应的速度或力。
 */
struct C_D_PlayerInput {
    bool up    = false; ///< 向前进输入
    bool down  = false; ///< 向后退输入
    bool left  = false; ///< 向左输入
    bool right = false; ///< 向右输入
};

} // namespace ECS
