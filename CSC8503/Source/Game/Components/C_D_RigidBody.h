#pragma once
#include <cstdint>

/**
 * @brief 刚体物理组件（数据组件）
 *
 * 存储 Jolt Physics Body 的 ID 和物理参数。
 * 与 C_D_Collider 配合使用，由 Sys_Physics 在 OnAwake/OnUpdate 中
 * 自动创建对应的 Jolt Body。
 *
 * @note POD 结构体，不含任何 Jolt 类型（解耦依赖）。
 *       Jolt BodyID 以 uint32_t 存储（与 JPH::BodyID::GetIndexAndSequenceNumber() 对应）。
 */
struct C_D_RigidBody {
    // --- Jolt 内部 ID（由 Sys_Physics 写入，外部只读）---
    uint32_t jolt_body_id   = 0xFFFFFFFF; ///< Jolt BodyID 原始值，0xFFFFFFFF 表示尚未创建

    // --- 物理参数（创建前可配置）---
    float    mass           = 1.0f;       ///< 质量（kg），0 表示静态体（isStatic=true 时忽略）
    float    linear_damping = 0.05f;      ///< 线性阻尼（0=无阻力，1=立即停止）
    float    angular_damping= 0.05f;      ///< 角阻尼
    float    gravity_factor = 1.0f;       ///< 重力缩放因子（0=无重力，负值=反重力）

    // --- 运动类型 ---
    // 优先级约定：is_static=true 时始终按静态体处理，is_kinematic 在该情况下被忽略。
    bool     is_static      = false;      ///< 静态体（不受任何力影响，不可移动）
    bool     is_kinematic   = false;      ///< 运动学体（由代码直接设置位置，不受物理力影响）

    // --- 旋转锁定（通常 2.5D 游戏锁定 X/Z 轴旋转）---
    // 2.5D 常见配置：lock_rotation_x=true 且 lock_rotation_z=true，仅允许绕 Y 轴旋转。
    bool     lock_rotation_x = false;
    bool     lock_rotation_y = false;
    bool     lock_rotation_z = false;

    // --- 内部状态（Sys_Physics 使用）---
    bool     body_created   = false;      ///< 标记 Jolt Body 是否已创建
};
