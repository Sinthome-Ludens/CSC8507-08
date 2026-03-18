#pragma once
#include <cstdint>
#include <vector>
#include "Vector.h"

/**
 * @brief 碰撞体形状类型
 */
enum class ColliderType : uint8_t {
    Box     = 0,   ///< 轴对齐包围盒（AABB / OBB）
    Sphere  = 1,   ///< 球体
    Capsule = 2,   ///< 胶囊体（适合人形角色）
    TriMesh = 3,   ///< 三角网格（仅用于静态体）
};

enum class ColliderFitMode : uint8_t {
    Manual         = 0, ///< 直接使用 half_x / half_y / half_z
    MeshBoundsAuto = 1, ///< 从渲染 mesh 局部包围盒自动推导尺寸
};

/**
 * @brief 碰撞体组件（数据组件）
 *
 * 定义实体的碰撞形状和物理材质属性。
 * 必须与 C_D_RigidBody 配合使用，由 Sys_Physics 构建对应的 Jolt Shape。
 *
 * @note 尺寸参数的含义因 ColliderType 不同而不同（详见各字段注释）。
 */
struct C_D_Collider {
    ColliderType type         = ColliderType::Box;
    ColliderFitMode fit_mode  = ColliderFitMode::Manual;

    // --- 形状尺寸（含义取决于 type）---
    // Box:     (half_x, half_y, half_z) = 半尺寸
    // Sphere:  half_x = radius（half_y/z 忽略）
    // Capsule: half_x = radius, half_y = half_height（不含半球部分）
    float half_x = 0.5f;
    float half_y = 0.5f;
    float half_z = 0.5f;

    // --- 自动拟合参数 ---
    // MeshBoundsAuto:
    // Box     -> 使用 mesh 局部包围盒半尺寸
    // Sphere  -> 使用 max(half_x, half_y, half_z) 作为半径
    // Capsule -> 使用 max(half_x, half_z) 作为半径，half_y - radius 作为 capsule half_height
    float fit_padding = 0.0f; ///< 自动拟合后额外扩张的安全边距（米）

    // --- 物理材质 ---
    float friction    = 0.5f;   ///< 摩擦系数（0=无摩擦，1=高摩擦）
    float restitution = 0.0f;   ///< 弹性系数（0=完全非弹性，1=完全弹性）

    // --- 触发器模式 ---
    bool  is_trigger  = false;  ///< true = Trigger（只检测重叠，不产生物理响应）

    // --- TriMesh 专用（type == TriMesh 时有效）---
    std::vector<NCL::Maths::Vector3> triVerts;    ///< 顶点坐标（体局部空间）
    std::vector<int>                 triIndices;  ///< 三角形索引，每 3 个为一个三角形
};
