/**
 * @file C_D_RigidBody.h
 * @brief 刚体物理数据组件定义。
 *
 * @details
 * 该组件保存 ECS 层可配置的刚体参数和运行态标记，不暴露底层物理引擎 ID。
 */
#pragma once
#include <cstdint>

/**
 * @brief 刚体物理组件（数据组件）
 *
 * 存储刚体物理参数。
 * 与 C_D_Collider 配合使用，由 Sys_Physics 在 OnAwake/OnUpdate 中
 * 自动创建对应的 Jolt Body。
 *
 * @note POD 结构体，不含任何 Jolt 类型（解耦依赖）。
 */
struct C_D_RigidBody {
    // --- 物理参数（创建前可配置）---
    float    mass           = 1.0f;       ///< 质量（kg），0 表示静态体（isStatic=true 时忽略）
    float    linear_damping = 0.05f;      ///< 线性阻尼（0=无阻力，1=立即停止）
    float    angular_damping= 0.05f;      ///< 角阻尼
    float    gravity_factor = 1.0f;       ///< 重力缩放因子（0=无重力，负值=反重力）

    // --- 运动类型 ---
    bool     is_static      = false;      ///< 静态体（不受任何力影响，不可移动）
    bool     is_kinematic   = false;      ///< 运动学体（由代码直接设置位置，不受物理力影响）

    // --- 旋转锁定（通常 2.5D 游戏锁定 X/Z 轴旋转）---
    bool     lock_rotation_x = false;
    bool     lock_rotation_y = false;
    bool     lock_rotation_z = false;

    // --- 内部状态（Sys_Physics 使用）---
    bool     body_created   = false;      ///< 标记 Jolt Body 是否已创建
};
