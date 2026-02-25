#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 聊天对话逻辑系统
 *
 * 职责：
 *   - 根据 alertLevel 计算 chatMode（proactive/mixed/passive）
 *   - proactive 模式下 NPC 定时发送消息，提供回复选项
 *   - mixed 模式下 NPC 质疑，限时回复
 *   - passive 模式下仅紧急反应提示
 *   - 管理回复计时器，超时触发惩罚
 *   - 回复成功/失败影响 alertLevel
 *
 * 数据：
 *   读取 Res_GameplayState.alertLevel
 *   读写 Res_ChatState
 *
 * 执行优先级：450（AI之后、UI之前）
 */
class Sys_Chat : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;
};

} // namespace ECS
