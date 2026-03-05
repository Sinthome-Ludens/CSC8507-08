#pragma once
#include "Core/ECS/EntityID.h"

/**
 * @brief 敌人进入 Hunt 状态事件（即时分发）
 *
 * 由 Sys_EnemyAI 在任意敌人从非 Hunt 状态转换到 Hunt 状态时发布。
 * 监听者：全局警戒度系统、UI 提示、音效触发等。
 *
 * @note 使用即时发布模式（bus.publish<Evt_AI_EnemyEnteredHunt>），
 *       确保警戒度在同一帧内更新。
 */
struct Evt_AI_EnemyEnteredHunt {
    ECS::EntityID enemy_entity; ///< 进入 Hunt 状态的敌人实体 ID
};
