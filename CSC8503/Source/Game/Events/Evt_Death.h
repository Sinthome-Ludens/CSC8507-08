/**
 * @file Evt_Death.h
 * @brief 定义实体死亡事件。
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_Health.h"

namespace ECS {

/**
 * @brief 实体死亡事件（延迟分发）
 *
 * 由 Sys_DeathJudgment 在检测到 HP <= 0 时通过 publish_deferred 发布。
 * 监听者：Sys_Audio（死亡音效）、Sys_Particle（死亡特效）、UI 等。
 */
struct Evt_Death {
    EntityID  entity;     ///< 死亡的实体
    DeathType deathType;  ///< 死亡类型
};

} // namespace ECS
