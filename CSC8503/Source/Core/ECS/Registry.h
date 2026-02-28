/**
 * @file Registry.h
 * @brief ECS 核心注册表：实体生命周期、组件管理与 View 迭代
 *
 * @details
 * 本文件提供 ECS 框架的两个核心类：
 *
 * ---
 *
 * ## View\<Ts...\>
 *
 * 多组件迭代视图，仅遍历同时持有所有 Ts 组件的实体集合。
 *
 * ### 实现原理
 * 1. 构造时接收各 `ComponentPool<Ts>*` 指针，调用 `FindSmallestPool()` 选出元素最少的池作为**主池（Primary Pool）**。
 * 2. `Iterator` 在主池的实体列表上线性扫描；对每个实体，用 C++17 折叠表达式
 *    `(... && pool->Has(id))` 检查它是否同时出现在所有其他池中。
 * 3. `operator*()` 用包展开 `pool->Get(id)...` 一次性返回
 *    `std::tuple<EntityID, Ts&...>`，支持 C++17 结构化绑定。
 *
 * ### 复杂度
 * - 最优情况（主池极小）：O(k)，k 为主池大小。
 * - 最差情况：O(N * M)，N 为主池大小，M 为组件类型数量。
 *
 * ### 线程安全 / 迭代约束
 * 迭代过程中**不得**对 Registry 进行实体创建或立即销毁操作，以防组件池 resize 使
 * 迭代器失效。使用延迟销毁（`Destroy` + `ProcessPendingDestroy`）可规避此限制。
 *
 * ---
 *
 * ## Registry
 *
 * ECS 世界的核心数据库，管理实体、组件与全局资源。
 *
 * ### 实体 ID 生命周期
 * - **创建**：若空闲列表（`m_FreeList`）非空，复用最旧的槽位并保持其 Version；
 *   否则在 `m_Versions` 末尾追加新槽位（Version=0）。
 * - **延迟销毁** (`Destroy`)：将 EntityID 加入 `m_PendingDestroy`，不立即失效。
 *   调用 `ProcessPendingDestroy()` 后才真正移除组件并递增 Version。
 * - **立即销毁** (`DestroyImmediate`)：遍历所有池调用 `Remove`，递增 Version，
 *   将槽位压入空闲列表。
 * - **有效性校验** (`Valid`)：`m_Versions[GetIndex(id)] == GetVersion(id)`，
 *   悬空引用因 Version 不匹配而被立即识别。
 *
 * ### 组件存储
 * - 内部用 `std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>>`
 *   按类型键存储各组件池（类型擦除）。
 * - `GetOrCreatePool<T>()` 懒惰创建池（首次 Emplace 时触发）。
 * - `GetPool<T>()` 返回 nullptr 若池未创建，供只读方法安全使用。
 *
 * ### 全局资源（Context）
 * - 用 `std::any` 存储任意类型的单例资源（如 `Res_Time`、`Res_Input`）。
 * - `ctx<T>()` 懒惰创建并返回引用；`ctx_emplace<T>(args...)` 原位构造。
 * - `has_ctx<T>()` 用于检查资源是否已注册。
 *
 * ### API 摘要
 *
 * | 方法 | 作用 |
 * |------|------|
 * | `Create()` | 创建实体，返回 EntityID |
 * | `Destroy(id)` | 标记延迟销毁 |
 * | `DestroyImmediate(id)` | 立即销毁 |
 * | `Valid(id)` | 检查实体是否存活 |
 * | `Emplace<T>(id, args...)` | 添加组件（原位构造） |
 * | `Get<T>(id)` | 获取组件引用（必须存在） |
 * | `TryGet<T>(id)` | 获取组件指针（不存在返回 nullptr） |
 * | `Remove<T>(id)` | 移除组件 |
 * | `Has<T>(id)` | 检查单个组件是否存在 |
 * | `view<Ts...>()` | 获取多组件迭代视图 |
 * | `ctx<T>()` | 获取/懒惰创建全局资源 |
 * | `ctx_emplace<T>(args...)` | 原位构造全局资源 |
 * | `reserve<T>(n)` | 预分配组件池容量 |
 * | `ProcessPendingDestroy()` | 执行帧末延迟销毁 |
 * | `Clear()` | 清空所有实体与组件 |
 *
 * @see ComponentPool.h
 * @see EntityID.h
 */

#pragma once

#include "ComponentPool.h"
#include "Game/Utils/Assert.h"
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <any>
#include <vector>
#include <functional>
#include <tuple>
#include <cstddef>

namespace ECS {

// Forward declaration
template<typename... Ts>
class View;

// =============================================================================
// Registry
// =============================================================================

/**
 * @brief ECS 核心注册表，管理实体生命周期、组件存储与全局资源。
 *
 * Registry 是整个 ECS 框架的数据中心，不可复制，可移动。
 * 每个游戏场景（Scene）通常持有一个 Registry 实例。
 */
class Registry {
public:
    Registry()  = default;
    ~Registry() = default;

    Registry(const Registry&)            = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&)                 = default;
    Registry& operator=(Registry&&)      = default;

    // -------------------------------------------------------------------------
    // Entity Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief 创建一个新实体，返回其唯一 EntityID。
     * @details 优先复用空闲列表（`m_FreeList`）中的槽位；若列表为空则扩展
     *          `m_Versions` 数组追加新槽位（Version 初始化为 0）。
     * @return 新实体的 EntityID（Index + Version 打包）。
     */
    EntityID Create();

    /**
     * @brief 将实体加入延迟销毁队列，本帧仍可安全访问其组件。
     * @details 调用 `ProcessPendingDestroy()` 后才会真正移除组件并回收槽位。
     * @param entity 待销毁的实体，必须为合法实体，否则触发 GAME_ASSERT。
     */
    void Destroy(EntityID entity);

    /**
     * @brief 立即销毁实体：移除全部组件、递增 Version、回收槽位。
     * @warning 在迭代 View 期间调用会导致未定义行为（迭代器失效）。
     * @param entity 待销毁的实体，必须为合法实体，否则触发 GAME_ASSERT。
     */
    void DestroyImmediate(EntityID entity);

    /**
     * @brief 检查 EntityID 是否指向当前存活的实体。
     * @details 通过比较 `m_Versions[GetIndex(id)] == GetVersion(id)` 实现；
     *          Version 不匹配意味着该 EntityID 已过期（悬空引用）。
     * @param entity 待检查的实体 ID。
     * @return 实体存活且 ID 有效则返回 true。
     */
    bool Valid(EntityID entity) const;

    /**
     * @brief 返回当前存活的实体数量（不含已标记为延迟销毁的实体）。
     * @return 存活实体计数。
     */
    size_t EntityCount() const { return m_ActiveCount; }

    // -------------------------------------------------------------------------
    // Component Operations
    // -------------------------------------------------------------------------

    /**
     * @brief 为实体添加 T 类型组件，返回组件的可写引用。
     * @details 以 `T{std::forward<Args>(args)...}` 原位构造组件对象，再 move 进组件池。
     * @tparam T    组件类型。
     * @tparam Args 传递给 T 构造函数的参数类型包。
     * @param entity 目标实体，必须合法且未持有 T 组件（ComponentPool 中断言）。
     * @param args   转发给 T 构造函数的参数。
     * @return 已存储组件的左值引用。
     */
    template<typename T, typename... Args>
    T& Emplace(EntityID entity, Args&&... args);

    /**
     * @brief 获取实体的 T 组件引用（可写）。
     * @tparam T 组件类型。
     * @param entity 目标实体，必须合法且已持有 T 组件。
     * @return 组件的左值引用。
     */
    template<typename T>
    T& Get(EntityID entity);

    /**
     * @brief 获取实体的 T 组件引用（只读）。
     * @tparam T 组件类型。
     * @param entity 目标实体，必须合法且已持有 T 组件。
     * @return 组件的 const 左值引用。
     */
    template<typename T>
    const T& Get(EntityID entity) const;

    /**
     * @brief 尝试获取实体的 T 组件指针（可写），不存在时返回 nullptr。
     * @tparam T 组件类型。
     * @param entity 目标实体，无效实体同样返回 nullptr。
     * @return 指向组件的指针，或 nullptr。
     */
    template<typename T>
    T* TryGet(EntityID entity);

    /**
     * @brief 尝试获取实体的 T 组件指针（只读），不存在时返回 nullptr。
     * @tparam T 组件类型。
     * @param entity 目标实体，无效实体同样返回 nullptr。
     * @return 指向组件的 const 指针，或 nullptr。
     */
    template<typename T>
    const T* TryGet(EntityID entity) const;

    /**
     * @brief 移除实体的 T 类型组件（若不存在则为空操作）。
     * @tparam T 组件类型。
     * @param entity 目标实体，必须合法，否则触发 GAME_ASSERT。
     */
    template<typename T>
    void Remove(EntityID entity);

    /**
     * @brief 检查实体是否持有单个 T 类型组件。
     * @tparam T 组件类型。
     * @param entity 目标实体，无效实体返回 false。
     * @return 持有则返回 true。
     */
    template<typename T>
    bool Has(EntityID entity) const;

    /**
     * @brief 检查实体是否同时持有 T、T2 及 Ts... 所有组件类型（变参版）。
     * @tparam T   第一个组件类型。
     * @tparam T2  第二个组件类型（至少需要两个类型才匹配此重载）。
     * @tparam Ts  剩余组件类型包。
     * @param entity 目标实体。
     * @return 全部持有则返回 true，任一不存在则返回 false。
     */
    template<typename T, typename T2, typename... Ts>
    bool Has(EntityID entity) const;

    // -------------------------------------------------------------------------
    // View
    // -------------------------------------------------------------------------

    /**
     * @brief 构造并返回一个多组件 View，仅迭代同时持有所有 Ts 组件的实体。
     * @details 调用时会为未创建的池自动初始化（懒惰创建），但空池使得视图立即为空。
     * @tparam Ts 一组组件类型（至少一个）。
     * @return View<Ts...> 对象，可用于 range-for 或 `.each(callback)`。
     *
     * @code
     * // Range-for + 结构化绑定
     * for (auto&& [id, tf, rb] : registry.view<C_D_Transform, C_D_RigidBody>()) { ... }
     *
     * // Callback
     * registry.view<C_D_Transform>().each([](EntityID id, C_D_Transform& tf) { ... });
     * @endcode
     */
    template<typename... Ts>
    View<Ts...> view();

    // -------------------------------------------------------------------------
    // Context (Global Resources)
    // -------------------------------------------------------------------------

    /**
     * @brief 获取 T 类型全局资源的可写引用，不存在时懒惰创建默认值。
     * @tparam T 资源类型，需要默认构造函数。
     * @return T 类型资源的左值引用。
     */
    template<typename T>
    T& ctx();

    /**
     * @brief 获取 T 类型全局资源的只读引用（资源必须已注册）。
     * @tparam T 资源类型。
     * @return T 类型资源的 const 左值引用。
     * @pre 资源已通过 `ctx<T>()` 或 `ctx_emplace<T>(...)` 注册，否则触发 GAME_ASSERT。
     */
    template<typename T>
    const T& ctx() const;

    /**
     * @brief 原位构造 T 类型全局资源（会覆盖已存在的同类型资源）。
     * @tparam T    资源类型。
     * @tparam Args 转发给 T 构造函数的参数类型包。
     * @param args  构造参数。
     * @return 已构造资源的左值引用。
     */
    template<typename T, typename... Args>
    T& ctx_emplace(Args&&... args);

    /**
     * @brief 检查 T 类型全局资源是否已注册。
     * @tparam T 资源类型。
     * @return 已注册则返回 true。
     */
    template<typename T>
    bool has_ctx() const;

    /**
     * @brief 移除 T 类型全局资源。
     * @tparam T 资源类型。
     */
    template<typename T>
    void ctx_erase();

    // -------------------------------------------------------------------------
    // Pool Utilities
    // -------------------------------------------------------------------------

    /**
     * @brief 预分配 T 类型组件池的底层存储容量，减少后续 Emplace 的重新分配次数。
     * @tparam T 组件类型。
     * @param capacity 期望的最大组件数量。
     */
    template<typename T>
    void reserve(size_t capacity);

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief 执行帧末延迟销毁：遍历 `m_PendingDestroy` 列表，对每个仍合法的实体
     *        调用 `DestroyImmediate_Impl()`，然后清空列表。
     * @details 应在每帧所有 System 的 `OnUpdate` 完成后由 SystemManager 统一调用。
     */
    void ProcessPendingDestroy();

    /**
     * @brief 清空所有实体、组件池内容及延迟销毁队列（不销毁池对象本身）。
     * @details 场景切换时调用，可快速重置世界状态。
     */
    void Clear();

private:
    std::vector<uint32_t>  m_Versions;      ///< 槽位版本数组；下标=EntityID.Index，值=当前 Version。
    std::vector<uint32_t>  m_FreeList;      ///< 可复用的槽位下标栈（LIFO 顺序）。
    size_t                 m_ActiveCount = 0; ///< 当前存活实体计数。
    std::vector<EntityID>  m_PendingDestroy; ///< 帧末待销毁的实体列表。

    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_Pools;   ///< 所有组件池（类型擦除）。
    std::unordered_map<std::type_index, std::any>                        m_Context; ///< 全局资源仓库。

    /**
     * @brief 获取 T 类型组件池，若不存在则创建并插入 m_Pools。
     * @tparam T 组件类型。
     * @return 指向 ComponentPool<T> 的原始指针（所有权由 m_Pools 持有）。
     */
    template<typename T>
    ComponentPool<T>* GetOrCreatePool();

    /**
     * @brief 获取 T 类型组件池（只读），若池未创建则返回 nullptr。
     * @tparam T 组件类型。
     * @return 指向 ComponentPool<T> 的原始指针，或 nullptr。
     */
    template<typename T>
    ComponentPool<T>* GetPool() const;

    /**
     * @brief 立即销毁实体的内部实现（不校验 Valid，调用方负责）。
     * @details 遍历所有池调用 Remove，递增 Version（超出 VERSION_MASK 后回绕为 0），
     *          并将槽位下标压入 m_FreeList。
     * @param entity 要销毁的实体 ID。
     */
    void DestroyImmediate_Impl(EntityID entity);
};

// =============================================================================
// View<Ts...>
// =============================================================================

/**
 * @brief 多组件迭代视图，仅遍历同时持有所有 Ts 组件的实体。
 * @tparam Ts 一组或多组组件类型（至少一个）。
 */
template<typename... Ts>
class View {
    static_assert(sizeof...(Ts) > 0, "View<Ts...> requires at least one component type");

public:
    /**
     * @brief 前向迭代器，遍历主池实体列表并跳过缺少任何 Ts 组件的实体。
     */
    class Iterator {
    public:
        using value_type = std::tuple<EntityID, Ts&...>;

        /**
         * @brief 构造迭代器，初始化后自动推进到第一个有效位置。
         * @param index    主池实体列表中的起始下标。
         * @param entities 主池实体列表的指针（生命周期由 View 保证）。
         * @param pools    所有组件池的指针元组。
         */
        Iterator(size_t index, const std::vector<EntityID>* entities,
                 std::tuple<ComponentPool<Ts>*...> pools)
            : m_Index(index), m_Entities(entities), m_Pools(pools) {
            AdvanceToValid();
        }

        /**
         * @brief 解引用，返回 `std::tuple<EntityID, Ts&...>`，支持 C++17 结构化绑定。
         * @return 当前实体 ID 与各组件引用的元组。
         */
        value_type operator*() const {
            EntityID id = (*m_Entities)[m_Index];
            return std::tuple<EntityID, Ts&...>(
                id, std::get<ComponentPool<Ts>*>(m_Pools)->Get(id)...
            );
        }

        /**
         * @brief 前进到下一个满足所有组件条件的实体。
         * @return 自身引用。
         */
        Iterator& operator++() { ++m_Index; AdvanceToValid(); return *this; }

        bool operator!=(const Iterator& other) const { return m_Index != other.m_Index; }
        bool operator==(const Iterator& other) const { return m_Index == other.m_Index; }

    private:
        /**
         * @brief 持续前进直到当前位置有效或到达末尾。
         */
        void AdvanceToValid() {
            while (m_Index < m_Entities->size() && !IsCurrentValid()) {
                ++m_Index;
            }
        }

        /**
         * @brief 使用折叠表达式检查当前实体是否持有所有 Ts 组件。
         * @details `(... && pool->Has(id))` 对每个 Ts 展开 Has 调用，任一为 false 则短路返回 false。
         */
        bool IsCurrentValid() const {
            EntityID id = (*m_Entities)[m_Index];
            return (... && std::get<ComponentPool<Ts>*>(m_Pools)->Has(id));
        }

        size_t                          m_Index;
        const std::vector<EntityID>*    m_Entities;
        std::tuple<ComponentPool<Ts>*...> m_Pools;
    };

    /**
     * @brief 构造 View，接收各组件池指针，选出元素最少的池作为主池。
     * @param pools 各 ComponentPool<Ts> 的原始指针（所有权由 Registry 持有）。
     */
    explicit View(ComponentPool<Ts>*... pools)
        : m_Pools(pools...) {
        m_PrimaryEntities = FindSmallestPool();
    }

    /**
     * @brief 返回指向第一个有效实体的迭代器。
     */
    Iterator begin() const {
        if (!m_PrimaryEntities) {
            return Iterator(0, &m_EmptyEntities, m_Pools);
        }
        return Iterator(0, m_PrimaryEntities, m_Pools);
    }

    /**
     * @brief 返回末尾哨兵迭代器。
     */
    Iterator end() const {
        if (!m_PrimaryEntities) {
            return Iterator(0, &m_EmptyEntities, m_Pools);
        }
        return Iterator(m_PrimaryEntities->size(), m_PrimaryEntities, m_Pools);
    }

    /**
     * @brief 以回调方式遍历所有满足条件的实体及其组件。
     * @details 使用 `std::invoke` 确保兼容普通函数指针、lambda、成员函数指针。
     * @tparam Func 可调用类型，签名需为 `void(EntityID, Ts&...)`。
     * @param func  对每个满足条件的实体调用的回调函数。
     */
    template<typename Func>
    void each(Func&& func) const {
        const std::vector<EntityID>* entities = m_PrimaryEntities ? m_PrimaryEntities : &m_EmptyEntities;
        for (size_t i = 0; i < entities->size(); ++i) {
            EntityID id = (*entities)[i];
            if ((... && std::get<ComponentPool<Ts>*>(m_Pools)->Has(id))) {
                std::invoke(std::forward<Func>(func), id,
                            std::get<ComponentPool<Ts>*>(m_Pools)->Get(id)...);
            }
        }
    }

    /**
     * @brief 返回主池是否为空（快速判断视图是否有任何实体）。
     */
    bool empty() const {
        return !m_PrimaryEntities || m_PrimaryEntities->empty();
    }

private:
    /**
     * @brief 遍历元组中所有池，返回元素数量最少的那个池的实体列表指针。
     * @details 选择最小池可减少 `AdvanceToValid()` 中无效实体的检查次数。
     * @return 指向最小池实体列表的指针，若所有池均为 nullptr 则返回 nullptr。
     */
    const std::vector<EntityID>* FindSmallestPool() const {
        const std::vector<EntityID>* smallest = nullptr;
        size_t minSize = SIZE_MAX;
        auto check = [&](auto* pool) {
            if (pool && pool->Size() < minSize) {
                minSize   = pool->Size();
                smallest  = &pool->GetEntities();
            }
        };
        (check(std::get<ComponentPool<Ts>*>(m_Pools)), ...);
        return smallest;
    }

    std::tuple<ComponentPool<Ts>*...> m_Pools;
    const std::vector<EntityID>*      m_PrimaryEntities = nullptr;

    static inline std::vector<EntityID> m_EmptyEntities{}; ///< 所有池均为空时的哨兵列表。
};

// =============================================================================
// Registry Template Implementation
// =============================================================================

inline EntityID Registry::Create() {
    uint32_t index;
    uint32_t version;

    if (!m_FreeList.empty()) {
        index = m_FreeList.back();
        m_FreeList.pop_back();
        version = m_Versions[index];
    } else {
        index = static_cast<uint32_t>(m_Versions.size());
        GAME_ASSERT(index <= Entity::INDEX_MASK,
                    "Registry::Create - entity index limit reached (max 2^20)");
        m_Versions.push_back(0u);
        version = 0u;
    }

    ++m_ActiveCount;
    return Entity::Make(index, version);
}

inline void Registry::Destroy(EntityID entity) {
    GAME_ASSERT(Valid(entity), "Registry::Destroy - invalid or already-destroyed entity");
    m_PendingDestroy.push_back(entity);
}

inline void Registry::DestroyImmediate(EntityID entity) {
    GAME_ASSERT(Valid(entity), "Registry::DestroyImmediate - invalid entity");
    DestroyImmediate_Impl(entity);
}

inline bool Registry::Valid(EntityID entity) const {
    if (!Entity::IsValid(entity)) { return false; }
    uint32_t index = Entity::GetIndex(entity);
    if (index >= static_cast<uint32_t>(m_Versions.size())) { return false; }
    return m_Versions[index] == Entity::GetVersion(entity);
}

inline void Registry::DestroyImmediate_Impl(EntityID entity) {
    for (auto& [type, pool] : m_Pools) {
        if (pool->Has(entity)) { pool->Remove(entity); }
    }
    uint32_t index    = Entity::GetIndex(entity);
    uint32_t newVer   = (m_Versions[index] + 1u) & Entity::VERSION_MASK;
    m_Versions[index] = newVer;
    m_FreeList.push_back(index);
    --m_ActiveCount;
}

inline void Registry::ProcessPendingDestroy() {
    for (EntityID id : m_PendingDestroy) {
        if (Valid(id)) { DestroyImmediate_Impl(id); }
    }
    m_PendingDestroy.clear();
}

inline void Registry::Clear() {
    m_PendingDestroy.clear();
    for (auto& [type, pool] : m_Pools) { pool->Clear(); }
    m_Versions.clear();
    m_FreeList.clear();
    m_ActiveCount = 0;
}

template<typename T>
ComponentPool<T>* Registry::GetOrCreatePool() {
    auto key = std::type_index(typeid(T));
    auto it  = m_Pools.find(key);
    if (it == m_Pools.end()) {
        it = m_Pools.emplace(key, std::make_unique<ComponentPool<T>>()).first;
    }
    return static_cast<ComponentPool<T>*>(it->second.get());
}

template<typename T>
ComponentPool<T>* Registry::GetPool() const {
    auto key = std::type_index(typeid(T));
    auto it  = m_Pools.find(key);
    if (it == m_Pools.end()) { return nullptr; }
    return static_cast<ComponentPool<T>*>(it->second.get());
}

template<typename T, typename... Args>
T& Registry::Emplace(EntityID entity, Args&&... args) {
    GAME_ASSERT(Valid(entity), "Registry::Emplace - invalid entity");
    return GetOrCreatePool<T>()->Emplace(entity, T{std::forward<Args>(args)...});
}

template<typename T>
T& Registry::Get(EntityID entity) {
    GAME_ASSERT(Valid(entity), "Registry::Get - invalid entity");
    return GetOrCreatePool<T>()->Get(entity);
}

template<typename T>
const T& Registry::Get(EntityID entity) const {
    GAME_ASSERT(Valid(entity), "Registry::Get (const) - invalid entity");
    const auto* pool = GetPool<T>();
    GAME_ASSERT(pool != nullptr, "Registry::Get (const) - component pool not found");
    return pool->Get(entity);
}

template<typename T>
T* Registry::TryGet(EntityID entity) {
    if (!Valid(entity)) { return nullptr; }
    auto* pool = GetPool<T>();
    if (!pool || !pool->Has(entity)) { return nullptr; }
    return &pool->Get(entity);
}

template<typename T>
const T* Registry::TryGet(EntityID entity) const {
    if (!Valid(entity)) { return nullptr; }
    const auto* pool = GetPool<T>();
    if (!pool || !pool->Has(entity)) { return nullptr; }
    return &pool->Get(entity);
}

template<typename T>
void Registry::Remove(EntityID entity) {
    GAME_ASSERT(Valid(entity), "Registry::Remove - invalid entity");
    auto* pool = GetPool<T>();
    if (pool) { pool->Remove(entity); }
}

template<typename T>
bool Registry::Has(EntityID entity) const {
    if (!Valid(entity)) { return false; }
    const auto* pool = GetPool<T>();
    return pool && pool->Has(entity);
}

template<typename T, typename T2, typename... Ts>
bool Registry::Has(EntityID entity) const {
    return Has<T>(entity) && Has<T2, Ts...>(entity);
}

template<typename... Ts>
View<Ts...> Registry::view() {
    return View<Ts...>(GetOrCreatePool<Ts>()...);
}

template<typename T>
T& Registry::ctx() {
    auto key = std::type_index(typeid(T));
    auto it  = m_Context.find(key);
    if (it == m_Context.end()) {
        it = m_Context.emplace(key, T{}).first;
    }
    return std::any_cast<T&>(it->second);
}

template<typename T>
const T& Registry::ctx() const {
    auto key = std::type_index(typeid(T));
    auto it  = m_Context.find(key);
    GAME_ASSERT(it != m_Context.end(),
                "Registry::ctx (const) - resource type not registered");
    return std::any_cast<const T&>(it->second);
}

template<typename T, typename... Args>
T& Registry::ctx_emplace(Args&&... args) {
    auto key = std::type_index(typeid(T));
    m_Context[key] = T{std::forward<Args>(args)...};
    return std::any_cast<T&>(m_Context[key]);
}

template<typename T>
bool Registry::has_ctx() const {
    return m_Context.find(std::type_index(typeid(T))) != m_Context.end();
}

template<typename T>
inline void Registry::ctx_erase() {
    m_Context.erase(std::type_index(typeid(T)));
}

template<typename T>
void Registry::reserve(size_t capacity) {
    GetOrCreatePool<T>()->GetDense().reserve(capacity);
}

} // namespace ECS
