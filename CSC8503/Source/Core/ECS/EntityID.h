/**
 * @file EntityID.h
 * @brief ECS 实体唯一标识符系统
 *
 * @details
 * 本文件定义了 ECS 框架中实体的唯一标识符类型 `EntityID` 及其操作工具函数。
 *
 * ## 位域布局（32 位整数）
 *
 * ```
 * 31       12 11          0
 * +---------+------------+
 * | Version |   Index    |
 * | 12 bits |  20 bits   |
 * +---------+------------+
 * ```
 *
 * - **Index（低 20 位）**：实体在注册表内部数组中的槽位下标，最多支持 2^20 = 1,048,576 个并发实体。
 * - **Version（高 12 位）**：槽位复用计数器，每次同一槽位被重新分配时递增，最多 4096 次复用。
 *   当外部持有的旧 EntityID 被用于查询时，Version 不匹配可立即识别出悬空引用（Dangling Reference）。
 *
 * ## 空实体约定
 * `NULL_ENTITY = 0xFFFFFFFF`。其 Index 和 Version 字段均为全 1，不会与任何合法实体冲突。
 * 所有未初始化的 EntityID 字段应默认赋值为 `NULL_ENTITY`。
 *
 * ## 操作函数（Entity 命名空间）
 *
 * | 函数 | 作用 |
 * |------|------|
 * | `GetIndex(id)` | 提取低 20 位槽位下标 |
 * | `GetVersion(id)` | 提取高 12 位版本号 |
 * | `Make(index, version)` | 从 index 和 version 合成 EntityID |
 * | `IsValid(id)` | 判断是否为合法非空实体 |
 *
 * ## 使用示例
 * @code
 * ECS::EntityID id = registry.Create();
 * uint32_t idx = ECS::Entity::GetIndex(id);   // 槽位
 * uint32_t ver = ECS::Entity::GetVersion(id); // 版本
 * bool ok = ECS::Entity::IsValid(id);         // true
 * ECS::EntityID null = ECS::Entity::NULL_ENTITY;
 * bool bad = ECS::Entity::IsValid(null);      // false
 * @endcode
 *
 * @note 本文件无外部依赖，仅包含 <cstdint>，可被整个 ECS 框架安全地作为最底层头文件引用。
 */

#pragma once

#include <cstdint>

namespace ECS {

/// @brief 实体唯一标识符类型，32 位无符号整数，高 12 位为 Version，低 20 位为 Index。
using EntityID = uint32_t;

/// @brief EntityID 位域操作工具函数集合。
namespace Entity {

    /// @brief Index 占用的位数（低位）。
    static constexpr uint32_t INDEX_BITS = 20;

    /// @brief Version 占用的位数（高位）。
    static constexpr uint32_t VERSION_BITS = 12;

    /// @brief 提取 Index 的位掩码：低 20 位全 1。
    static constexpr uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1u;

    /// @brief 提取 Version 的位掩码：12 位全 1。
    static constexpr uint32_t VERSION_MASK = (1u << VERSION_BITS) - 1u;

    /// @brief 空实体哨兵值，所有位均为 1（0xFFFFFFFF），表示"无效/未设置"。
    static constexpr EntityID NULL_ENTITY = ~0u;

    /**
     * @brief 从 EntityID 中提取槽位下标（低 20 位）。
     * @param id 目标实体 ID，可以是 NULL_ENTITY。
     * @return 槽位下标，范围 [0, 2^20)。
     */
    inline uint32_t GetIndex(EntityID id) {
        return id & INDEX_MASK;
    }

    /**
     * @brief 从 EntityID 中提取版本号（高 12 位）。
     * @param id 目标实体 ID，可以是 NULL_ENTITY。
     * @return 版本号，范围 [0, 2^12)。
     */
    inline uint32_t GetVersion(EntityID id) {
        return (id >> INDEX_BITS) & VERSION_MASK;
    }

    /**
     * @brief 合成 EntityID：将槽位下标与版本号打包到一个 32 位整数中。
     * @param index   槽位下标，仅低 20 位有效，多余位被截断。
     * @param version 版本号，仅低 12 位有效，多余位被截断。
     * @return 合成后的 EntityID。
     */
    inline EntityID Make(uint32_t index, uint32_t version) {
        return ((version & VERSION_MASK) << INDEX_BITS) | (index & INDEX_MASK);
    }

    /**
     * @brief 判断 EntityID 是否为合法（非空）实体。
     * @param id 待检查的实体 ID。
     * @return 若 id 不等于 NULL_ENTITY 则返回 true。
     */
    inline bool IsValid(EntityID id) {
        return id != NULL_ENTITY;
    }

} // namespace Entity

} // namespace ECS
