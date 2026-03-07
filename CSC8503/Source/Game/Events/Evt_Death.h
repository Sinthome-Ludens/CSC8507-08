#pragma once
#include "Core/ECS/EntityID.h"
#include <cstdint>

/**
 * @brief 实体死亡事件（延迟分发）
 *
 * 由 Sys_DeathJudgment 在检测到 HP <= 0 时通过 publish_deferred 发布。
 * 监听者：Sys_Audio（死亡音效）、Sys_Particle（死亡特效）、UI 等。
 */
struct Evt_Death {
    ECS::EntityID entity;      ///< 死亡的实体
    uint8_t       deathType;   ///< 0=玩家被捕, 1=玩家HP归零, 2=玩家触发区死亡, 3=敌人HP归零
};
