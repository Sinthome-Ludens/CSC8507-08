/**
 * @file ComponentPool.h
 * @brief ECS 组件存储池（稀疏集合实现）
 *
 * @details
 * 本文件提供两个类：
 *
 * - **IComponentPool**：类型擦除的组件池抽象基类，允许 Registry 以多态方式管理所有类型的组件池。
 * - **ComponentPool\<T\>**：具体类型的组件存储池，采用**稀疏集合（Sparse Set）**数据结构实现。
 *
 * ## 稀疏集合数据结构
 *
 * 稀疏集合由三条并行数组组成：
 *
 * ```
 * m_Sparse（稀疏数组）
 *   下标 = EntityID 的 Index 字段
 *   值   = 该实体在 m_Dense 中的位置，SPARSE_INVALID 表示不存在
 *
 * m_Dense（稠密数组，连续内存）
 *   下标 = 稠密索引
 *   值   = T 类型组件数据
 *
 * m_DenseEntities（稠密实体数组，与 m_Dense 并行）
 *   下标 = 稠密索引
 *   值   = 拥有该组件的 EntityID
 * ```
 *
 * 三条数组的对应关系满足：
 * - `m_Sparse[Entity::GetIndex(e)] == i`  ←→  `m_DenseEntities[i] == e`  ←→  `m_Dense[i]` 是实体 e 的组件
 *
 * ## 时间复杂度
 *
 * | 操作 | 复杂度 |
 * |------|--------|
 * | `Emplace` | O(1) 均摊 |
 * | `Get` | O(1) |
 * | `Has` | O(1) |
 * | `Remove` | O(1)，通过 swap-and-pop 算法 |
 * | 迭代全部组件 | O(n)，缓存友好的连续内存 |
 *
 * ## Swap-and-Pop 删除算法
 *
 * 删除实体 e 时：
 * 1. 找到 e 在 m_Dense 中的位置 denseIndex。
 * 2. 将 m_Dense.back() 和 m_DenseEntities.back() 搬到 denseIndex 处（O(1) 移动）。
 * 3. 更新被移动实体在 m_Sparse 中的映射。
 * 4. pop_back() 弹出最后一个槽（现已是重复数据）。
 * 5. 将 m_Sparse[Entity::GetIndex(e)] 置为 SPARSE_INVALID。
 *
 * @note
 * - `Emplace` 要求实体尚未拥有该组件，否则触发 GAME_ASSERT。
 * - `Get` 要求实体已拥有该组件，否则触发 GAME_ASSERT。
 * - `Remove` 在实体不存在时为空操作（no-op）。
 * - 模板实现直接写在头文件中，避免显式实例化的繁琐。
 *
 * @see EntityID.h
 * @see Registry.h
 */

#pragma once

#include "EntityID.h"
#include "Game/Utils/Assert.h"
#include <vector>
#include <cstddef>

namespace ECS {

/**
 * @brief 类型擦除的组件池抽象基类。
 *
 * @details
 * Registry 通过 `std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>>`
 * 统一持有所有具体类型的组件池，需要多态操作时通过此接口进行。
 */
class IComponentPool {
public:
    virtual ~IComponentPool() = default;

    /**
     * @brief 移除指定实体的组件（若不存在则为空操作）。
     * @param entity 目标实体 ID。
     */
    virtual void Remove(EntityID entity) = 0;

    /**
     * @brief 判断指定实体是否拥有此类型的组件。
     * @param entity 目标实体 ID。
     * @return 拥有则返回 true。
     */
    virtual bool Has(EntityID entity) const = 0;

    /**
     * @brief 返回当前存储的组件数量。
     * @return 组件数量（等于拥有该组件的实体数）。
     */
    virtual size_t Size() const = 0;

    /**
     * @brief 清空所有组件和映射关系（不释放底层内存）。
     */
    virtual void Clear() = 0;

    /**
     * @brief 返回稠密数组中所有拥有该组件的实体 ID 列表（只读）。
     * @return 实体 ID 向量的 const 引用。
     */
    virtual const std::vector<EntityID>& GetEntities() const = 0;
};

/**
 * @brief 具体类型组件存储池，基于稀疏集合实现。
 * @tparam T 被存储的组件类型，需要可移动构造（MoveConstructible）。
 */
template<typename T>
class ComponentPool final : public IComponentPool {
public:

    /**
     * @brief 为实体添加一个组件，并返回其引用。
     * @details 若 T 提供默认构造函数，可直接调用 `Emplace(entity)` 得到默认值组件。
     * @param entity    目标实体 ID，必须为合法实体且尚未持有 T 组件。
     * @param component 要存储的组件值（右值，将被 move 进稠密数组）。
     * @return 已存储组件的左值引用，可用于后续就地修改。
     * @pre `!Has(entity)`，否则触发 GAME_ASSERT。
     */
    T& Emplace(EntityID entity, T component = T{});

    /**
     * @brief 获取指定实体的组件引用（可写）。
     * @param entity 目标实体 ID，必须已持有 T 组件。
     * @return 组件的左值引用。
     * @pre `Has(entity)`，否则触发 GAME_ASSERT。
     */
    T& Get(EntityID entity);

    /**
     * @brief 获取指定实体的组件引用（只读）。
     * @param entity 目标实体 ID，必须已持有 T 组件。
     * @return 组件的 const 左值引用。
     * @pre `Has(entity)`，否则触发 GAME_ASSERT。
     */
    const T& Get(EntityID entity) const;

    /**
     * @brief 移除指定实体的组件，使用 swap-and-pop 算法保证 O(1) 复杂度。
     * @param entity 目标实体 ID。若实体不持有组件，则为空操作。
     */
    void Remove(EntityID entity) override;

    /**
     * @brief 判断指定实体是否持有 T 类型组件。
     * @param entity 目标实体 ID。
     * @return 持有则返回 true。
     */
    bool Has(EntityID entity) const override;

    /**
     * @brief 返回当前组件数量。
     * @return m_Dense 数组的大小。
     */
    size_t Size() const override { return m_Dense.size(); }

    /**
     * @brief 清空所有组件数据和稀疏映射（不释放底层 vector 内存）。
     */
    void Clear() override;

    /**
     * @brief 返回稠密数组对应的实体 ID 列表（只读），用于 View 迭代。
     * @return 实体 ID 向量的 const 引用。
     */
    const std::vector<EntityID>& GetEntities() const override { return m_DenseEntities; }

    /**
     * @brief 返回组件稠密数组的可写引用，供高级批量操作使用。
     * @return 组件 T 向量的引用。
     */
    std::vector<T>& GetDense() { return m_Dense; }

private:
    /// @brief 稀疏数组中表示"实体无此组件"的哨兵值。
    static constexpr uint32_t SPARSE_INVALID = ~0u;

    std::vector<T>        m_Dense;         ///< 连续存储的组件数据（缓存友好）。
    std::vector<EntityID> m_DenseEntities; ///< 与 m_Dense 并行，记录每个槽对应的 EntityID。
    std::vector<uint32_t> m_Sparse;        ///< 稀疏数组：EntityID.Index → m_Dense 下标。

    /**
     * @brief 确保稀疏数组容量足以容纳给定的实体下标。
     * @details 扩容时新槽填充 SPARSE_INVALID。
     * @param entityIndex 实体的 Index 字段值（EntityID & INDEX_MASK）。
     */
    void EnsureSparse(uint32_t entityIndex);

    /**
     * @brief 查询实体在稠密数组中的下标，不存在时返回 SPARSE_INVALID。
     * @param entity 目标实体 ID。
     * @return 稠密数组下标，或 SPARSE_INVALID。
     */
    uint32_t SparseGet(EntityID entity) const;
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
void ComponentPool<T>::EnsureSparse(uint32_t entityIndex) {
    if (entityIndex >= static_cast<uint32_t>(m_Sparse.size())) {
        m_Sparse.resize(static_cast<size_t>(entityIndex) + 1, SPARSE_INVALID);
    }
}

template<typename T>
uint32_t ComponentPool<T>::SparseGet(EntityID entity) const {
    uint32_t idx = Entity::GetIndex(entity);
    if (idx >= static_cast<uint32_t>(m_Sparse.size())) {
        return SPARSE_INVALID;
    }
    return m_Sparse[idx];
}

template<typename T>
T& ComponentPool<T>::Emplace(EntityID entity, T component) {
    GAME_ASSERT(!Has(entity), "ComponentPool::Emplace - entity already owns this component type");
    uint32_t entityIndex = Entity::GetIndex(entity);
    EnsureSparse(entityIndex);
    m_Sparse[entityIndex] = static_cast<uint32_t>(m_Dense.size());
    m_Dense.push_back(std::move(component));
    m_DenseEntities.push_back(entity);
    return m_Dense.back();
}

template<typename T>
T& ComponentPool<T>::Get(EntityID entity) {
    GAME_ASSERT(Has(entity), "ComponentPool::Get - entity does not own this component type");
    return m_Dense[m_Sparse[Entity::GetIndex(entity)]];
}

template<typename T>
const T& ComponentPool<T>::Get(EntityID entity) const {
    GAME_ASSERT(Has(entity), "ComponentPool::Get (const) - entity does not own this component type");
    return m_Dense[m_Sparse[Entity::GetIndex(entity)]];
}

template<typename T>
void ComponentPool<T>::Remove(EntityID entity) {
    if (!Has(entity)) { return; }

    uint32_t entityIndex  = Entity::GetIndex(entity);
    uint32_t denseIndex   = m_Sparse[entityIndex];
    uint32_t lastIndex    = static_cast<uint32_t>(m_Dense.size()) - 1u;

    /// @note Swap-and-pop：将最后一个元素移到被删位置，再弹出末尾，保持稠密连续
    if (denseIndex != lastIndex) {
        m_Dense[denseIndex]        = std::move(m_Dense[lastIndex]);
        EntityID movedEntity       = m_DenseEntities[lastIndex];
        m_DenseEntities[denseIndex] = movedEntity;
        m_Sparse[Entity::GetIndex(movedEntity)] = denseIndex;
    }

    m_Dense.pop_back();
    m_DenseEntities.pop_back();
    m_Sparse[entityIndex] = SPARSE_INVALID;
}

template<typename T>
bool ComponentPool<T>::Has(EntityID entity) const {
    return SparseGet(entity) != SPARSE_INVALID;
}

template<typename T>
void ComponentPool<T>::Clear() {
    m_Dense.clear();
    m_DenseEntities.clear();
    m_Sparse.clear();
}

} // namespace ECS
