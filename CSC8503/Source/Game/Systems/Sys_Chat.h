/**
 * @file Sys_Chat.h
 * @brief Chat system — dialogue tree logic, reply effects, and timeout penalties
 */
#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 对话逻辑系统 — 3 模式对话树 / 回复效果 / 超时惩罚
 *
 * 根据 alertLevel 自动切换 chatMode (proactive/mixed/passive)，
 * 按预设对话树推送 NPC 消息和回复选项，处理玩家回复效果。
 *
 * 注册优先级：450（在 Countdown(350) 之后、UI(500) 之前）
 * ECS 合规：System 无成员变量，仅读写 Res_ChatState + Res_GameState + Res_UIState
 */
class Sys_Chat : public ISystem {
public:
    void OnAwake (Registry& registry)           override;
    void OnUpdate(Registry& registry, float dt) override;
};

} // namespace ECS
