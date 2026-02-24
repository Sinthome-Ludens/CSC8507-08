#pragma once

#include <cstdint>

/**
 * @brief 远程游戏动作事件（延迟分发）
 *
 * 由 Sys_Network 收到类型为 GAME_ACTION 的网络包时发布。
 * 包含动作来源实体、动作目标实体，以及动作代码与参数。
 * 监听者：Sys_Combat, Sys_Interact, Sys_Audio 等。
 *
 * @note 使用延迟发布模式（bus.publish_deferred<Evt_Net_GameAction>），
 *       确保统一网络包带来的事件在相同逻辑帧被消费。
 */
struct Evt_Net_GameAction {
    uint32_t sourceNetID; ///< 发起动作的实体的网络 ID
    uint32_t targetNetID; ///< 动作目标实体的网络 ID（如有，0 为无效/空）
    uint8_t  actionCode;  ///< 动作类型代码（如：开火、跳跃、交互等）
    int32_t  param1;      ///< 补充参数（如伤害值、技能 ID 等）
};
