#pragma once
#include "Core/ECS/EntityID.h"
#include "Vector.h"

using namespace NCL::Maths;

/**
 * @brief 刚体碰撞事件（即时分发）
 *
 * 由 Sys_Physics 的 ContactListener 在两个非 Trigger 刚体发生碰撞时发布。
 * 监听者：Sys_Gameplay, Sys_Audio, Sys_Particle 等。
 *
 * @note 使用即时发布模式（bus.publish<Evt_Phys_Collision>），
 *       确保物理因果链（碰撞 -> 伤害 -> 死亡）在同一帧内闭环。
 */
struct Evt_Phys_Collision {
    ECS::EntityID entity_a;           ///< 碰撞体 A
    ECS::EntityID entity_b;           ///< 碰撞体 B
    Vector3       contact_point;      ///< 世界空间接触点（第一个接触点）
    Vector3       contact_normal;     ///< 从 B 指向 A 的接触法线（单位向量）
    float         separating_velocity; ///< 分离速度（正值表示碰撞冲量大小）
};
