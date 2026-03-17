/**
 * @file Evt_EnemyAlertChange.h
 * @brief 敌人警戒状态变化事件（延迟分发）。
 *
 * 由 Sys_EnemyAI 在敌人进入 Hunt 状态时发布。
 * 订阅者（同 Sys_EnemyAI）用于实现敌人间联动：
 * 附近敌人收到事件后 boost detection_value，模拟呼叫增援行为。
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"
#include "Game/Components/Res_EnemyEnums.h"

namespace ECS {

struct Evt_EnemyAlertChange {
    EntityID       source;
    EnemyState     newState;
    NCL::Maths::Vector3 position;
};

} // namespace ECS
