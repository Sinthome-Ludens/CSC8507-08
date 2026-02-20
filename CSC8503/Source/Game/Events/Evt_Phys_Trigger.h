#pragma once
#include "Core/ECS/EntityID.h"

/**
 * @brief Trigger 进入事件（即时分发）
 *
 * 由 Sys_Physics 的 ContactListener 在一个 Trigger 与另一个刚体首次接触时发布。
 * 前提：C_D_Collider::isTrigger == true。
 */
struct Evt_Phys_TriggerEnter {
    ECS::EntityID entity_trigger;  ///< 拥有 Trigger 的实体
    ECS::EntityID entity_other;    ///< 进入 Trigger 的实体
};

/**
 * @brief Trigger 离开事件（即时分发）
 *
 * 由 Sys_Physics 的 ContactListener 在之前接触的实体离开 Trigger 时发布。
 */
struct Evt_Phys_TriggerExit {
    ECS::EntityID entity_trigger;  ///< 拥有 Trigger 的实体
    ECS::EntityID entity_other;    ///< 离开 Trigger 的实体
};
