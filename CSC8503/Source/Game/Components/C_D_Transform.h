/**
 * @file C_D_Transform.h
 * @brief 变换组件：存储实体的空间坐标信息（位置、旋转、缩放）
 *
 * @details
 * `C_D_Transform` 是 ECS 中最基础的空间数据组件，几乎所有可见实体都需要挂载此组件。
 *
 * ## 坐标系统
 *
 * 本项目采用 **右手坐标系**（与 OpenGL 一致）：
 * - **+X**：向右
 * - **+Y**：向上
 * - **+Z**：朝向观察者（屏幕外）
 *
 * ## 数据字段
 *
 * | 字段 | 类型 | 说明 |
 * |------|------|------|
 * | `position` | `NCL::Maths::Vector3` | 世界坐标位置（米） |
 * | `rotation` | `NCL::Maths::Quaternion` | 旋转四元数（归一化） |
 * | `scale` | `NCL::Maths::Vector3` | 缩放因子（1.0 = 原始大小） |
 *
 * ## 设计约束
 *
 * 1. **POD 结构体**：所有字段均为 NCL 数学类型（连续内存布局），支持 `memcpy` 快速拷贝。
 * 2. **无层级关系**：ECS 中不维护父子变换树（Parent-Child Hierarchy）。
 *    若需要层级关系，应在 System 中手动实现（如 `Sys_Hierarchy`）。
 * 3. **初始化约定**：
 *    - `position = (0, 0, 0)` - 世界原点
 *    - `rotation = (0, 0, 0, 1)` - 单位四元数（无旋转）
 *    - `scale = (1, 1, 1)` - 原始大小
 *
 * ## 使用场景
 *
 * - **Sys_Physics**：同步 Jolt 物理体位置到 `position` 和 `rotation`
 * - **Sys_Render**：构造模型矩阵 `Model = T * R * S`，提交给渲染器
 * - **Sys_Camera**：读取玩家 Transform 实现相机跟随
 * - **Sys_AI**：读取敌人/玩家位置进行导航和视野判定
 *
 * ## 性能考量
 *
 * - **内存布局**：48 字节（Vector3 * 2 + Quaternion）
 * - **缓存友好**：连续存储于 ComponentPool，迭代时高效
 * - **避免频繁构造**：使用默认成员初始化，减少 Emplace 开销
 *
 * @note 四元数 `rotation` 应始终保持归一化（Normalized）。
 *       若在运行时累积旋转，需定期调用 `rotation.Normalise()`。
 *
 * @see C_D_RigidBody (物理组件，与 Transform 同步)
 * @see Sys_Render (读取 Transform 构造渲染矩阵)
 */

#pragma once

#include "Vector.h"
#include "Quaternion.h"

namespace ECS {

/**
 * @brief 变换组件：实体的空间位置、旋转与缩放。
 *
 * @details
 * 所有需要在 3D 空间中定位的实体（玩家、敌人、道具、相机）都应挂载此组件。
 * 数据由 Sys_Physics 写入，由 Sys_Render 读取。
 */
struct C_D_Transform {
    NCL::Maths::Vector3    position{0.0f, 0.0f, 0.0f}; ///< 世界坐标位置（米）
    NCL::Maths::Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f}; ///< 旋转四元数（归一化）
    NCL::Maths::Vector3    scale{1.0f, 1.0f, 1.0f};    ///< 缩放因子（各轴独立）
};

} // namespace ECS
