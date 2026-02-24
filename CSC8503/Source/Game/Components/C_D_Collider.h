/**
 * @file C_D_Collider.h
 * @brief 碰撞体数据组件与查询过滤位定义。
 *
 * @details
 * 本文件定义 ECS 碰撞体组件 `C_D_Collider`，用于描述形状、材质、触发器属性，
 * 以及空间查询使用的双轨过滤位：`layer_mask` 与 `tag_mask`。
 *
 * @note 位扩展规则采用“只新增不重排”：已使用位不可复用或改语义。
 */

#pragma once
#include <cstdint>

/**
 * @brief 碰撞体形状类型
 */
enum class ColliderType : uint8_t {
    Box     = 0,   ///< 轴对齐包围盒（AABB / OBB）
    Sphere  = 1,   ///< 球体
    Capsule = 2,   ///< 胶囊体（适合人形角色）
};

/**
 * @brief 碰撞体组件（数据组件）
 *
 * 定义实体的碰撞形状和物理材质属性。
 * 必须与 C_D_RigidBody 配合使用，由 Sys_Physics 构建对应的 Jolt Shape。
 *
 * @note 尺寸参数的含义因 ColliderType 不同而不同（详见各字段注释）。
 *       POD 结构体，不含任何 Jolt 类型。
 */
struct C_D_Collider {
    ColliderType type         = ColliderType::Box;

    // --- 形状尺寸（含义取决于 type）---
    // Box:     (half_x, half_y, half_z) = 半尺寸
    // Sphere:  half_x = radius（half_y/z 忽略）
    // Capsule: half_x = radius, half_y = half_height（不含半球部分）
    float half_x = 0.5f;
    float half_y = 0.5f;
    float half_z = 0.5f;

    // --- 物理材质 ---
    float friction    = 0.5f;   ///< 摩擦系数（0=无摩擦，1=高摩擦）
    float restitution = 0.0f;   ///< 弹性系数（0=完全非弹性，1=完全弹性）

    // --- 触发器模式 ---
    bool  is_trigger  = false;  ///< true = Trigger（只检测重叠，不产生物理响应）

    // --- 空间查询过滤位（双轨）---
    uint32_t layer_mask = 0xFFFFFFFFu; ///< 物理层过滤位（粗粒度）
    uint32_t tag_mask   = 0xFFFFFFFFu; ///< 语义标签过滤位（细粒度）
};
