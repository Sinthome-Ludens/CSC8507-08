/**
 * @file SystemManager.h
 * @brief ECS System 管理器：注册、排序与批量生命周期调度
 *
 * @details
 * `SystemManager` 是 ECS 框架的调度中心，负责管理所有 `ISystem` 实例的完整生命周期。
 *
 * ## 注册与排序
 *
 * - 通过 `Register<T>(priority)` 懒惰构造并注册系统；同一类型只能注册一次（GAME_ASSERT）。
 * - 系统按**优先级（priority）升序排列**——值越小越先执行。
 * - 注册后需调用 `Sort()` 使优先级变更生效；`Register` 内部已自动调用一次 Sort。
 *
 * ## 批量调度
 *
 * | 方法 | 作用 |
 * |------|------|
 * | `AwakeAll(registry)` | 对所有系统按序调用 `OnAwake` |
 * | `UpdateAll(registry, dt)` | 对所有系统按序调用 `OnUpdate` |
 * | `FixedUpdateAll(registry, fixedDt)` | 对所有系统按序调用 `OnFixedUpdate` |
 * | `DestroyAll(registry)` | 按**逆序**调用 `OnDestroy`（先初始化的后销毁） |
 *
 * ## 推荐主循环接入方式
 *
 * @code
 * // 初始化
 * ECS::SystemManager sm;
 * sm.Register<Sys_Physics>(100);
 * sm.Register<Sys_AI>(200);
 * sm.Register<Sys_Render>(400);
 * sm.AwakeAll(registry);
 *
 * // 主循环
 * while (running) {
 *     float dt = timer.GetDeltaTime();
 *     sm.UpdateAll(registry, dt);
 *
 *     accumulator += dt;
 *     while (accumulator >= FIXED_DT) {
 *         sm.FixedUpdateAll(registry, FIXED_DT);
 *         accumulator -= FIXED_DT;
 *     }
 *
 *     registry.ProcessPendingDestroy(); // 帧末清理
 * }
 *
 * sm.DestroyAll(registry);
 * @endcode
 *
 * ## 实现细节
 *
 * - 系统实例以 `std::unique_ptr<ISystem>` 存储（独占所有权）。
 * - `m_Systems` 为按优先级排序的有序列表（`std::vector<Entry>`）。
 * - `m_TypeSet` 用 `std::type_index` 作键，防止同类型重复注册。
 * - `Sort()` 使用 `std::stable_sort`，优先级相同的系统保持注册顺序（稳定排序）。
 *
 * @see BaseSystem.h
 * @see Registry.h
 */

#pragma once

#include "BaseSystem.h"
#include "Game/Utils/Assert.h"
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_set>
#include <algorithm>

namespace ECS {

/**
 * @brief ECS System 管理器，统一注册、排序和批量调用所有系统的生命周期接口。
 *
 * SystemManager 不可复制，可移动。通常与 Registry 成对存在于同一个 Scene 对象中。
 */
class SystemManager {
public:
    SystemManager()  = default;
    ~SystemManager() = default;

    SystemManager(const SystemManager&)            = delete;
    SystemManager& operator=(const SystemManager&) = delete;
    SystemManager(SystemManager&&)                 = default;
    SystemManager& operator=(SystemManager&&)      = default;

    /**
     * @brief 注册一个新 System，原位构造并按优先级插入。
     * @details 内部流程：
     *  1. 以 `typeid(T)` 检查是否已注册同类型（重复注册触发 GAME_ASSERT）。
     *  2. `std::make_unique<T>()` 构造系统实例。
     *  3. 追加到 `m_Systems` 并调用 `Sort()` 保持有序。
     * @tparam T 系统类型，需继承自 ISystem 且可默认构造。
     * @param priority 执行优先级（值越小越先执行，默认 0）。
     * @return 已注册系统实例的原始指针（所有权保留在 SystemManager 内）。
     */
    template<typename T>
    T* Register(int priority = 0);

    /**
     * @brief 获取已注册的 T 类型系统实例指针。
     * @tparam T 系统类型。
     * @return 指向 T 实例的指针，若未注册则返回 nullptr。
     */
    template<typename T>
    T* Get() const;

    /**
     * @brief 对所有系统按优先级升序调用 `OnAwake(registry)`。
     * @param registry 当前场景注册表，传递给每个系统。
     */
    void AwakeAll(Registry& registry);

    /**
     * @brief 对所有系统按优先级升序调用 `OnUpdate(registry, dt)`。
     * @param registry 当前场景注册表。
     * @param dt       本帧变步长时间间隔（秒）。
     */
    void UpdateAll(Registry& registry, float dt);

    /**
     * @brief 对所有系统按优先级升序调用 `OnFixedUpdate(registry, fixedDt)`。
     * @param registry 当前场景注册表。
     * @param fixedDt  固定物理步长（秒）。
     */
    void FixedUpdateAll(Registry& registry, float fixedDt);

    /**
     * @brief 按优先级**逆序**对所有系统调用 `OnDestroy(registry)`，然后清空注册表。
     * @details 逆序销毁确保高优先级（先初始化）的系统最后释放，
     *          使依赖关系的释放顺序与初始化顺序相反（LIFO）。
     * @param registry 当前场景注册表。
     */
    void DestroyAll(Registry& registry);

    /**
     * @brief 按 priority 升序对 m_Systems 执行稳定排序。
     * @details 优先级相同的系统保持 Register 调用顺序不变（std::stable_sort）。
     */
    void Sort();

    /**
     * @brief 返回当前注册的系统数量。
     * @return 系统数量。
     */
    size_t Count() const { return m_Systems.size(); }

private:
    /// @brief 系统注册条目，存储优先级与系统实例。
    struct Entry {
        int                      priority;
        std::type_index          typeIndex;
        std::unique_ptr<ISystem> system;

        Entry(int p, std::type_index ti, std::unique_ptr<ISystem> s)
            : priority(p), typeIndex(ti), system(std::move(s)) {}
    };

    std::vector<Entry>              m_Systems; ///< 按 priority 升序排列的系统列表。
    std::unordered_set<std::type_index> m_TypeSet; ///< 已注册类型集合，防止重复注册。
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename T>
T* SystemManager::Register(int priority) {
    static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");

    auto key = std::type_index(typeid(T));
    GAME_ASSERT(m_TypeSet.find(key) == m_TypeSet.end(),
                "SystemManager::Register - system type already registered");

    m_TypeSet.insert(key);
    auto ptr = std::make_unique<T>();
    T* raw   = ptr.get();
    m_Systems.emplace_back(priority, key, std::move(ptr));
    Sort();
    return raw;
}

template<typename T>
T* SystemManager::Get() const {
    auto key = std::type_index(typeid(T));
    for (const auto& entry : m_Systems) {
        if (entry.typeIndex == key) {
            return static_cast<T*>(entry.system.get());
        }
    }
    return nullptr;
}

inline void SystemManager::Sort() {
    std::stable_sort(m_Systems.begin(), m_Systems.end(),
                     [](const Entry& a, const Entry& b) {
                         return a.priority < b.priority;
                     });
}

inline void SystemManager::AwakeAll(Registry& registry) {
    for (auto& entry : m_Systems) {
        entry.system->OnAwake(registry);
    }
}

inline void SystemManager::UpdateAll(Registry& registry, float dt) {
    for (auto& entry : m_Systems) {
        entry.system->OnUpdate(registry, dt);
    }
}

inline void SystemManager::FixedUpdateAll(Registry& registry, float fixedDt) {
    for (auto& entry : m_Systems) {
        entry.system->OnFixedUpdate(registry, fixedDt);
    }
}

inline void SystemManager::DestroyAll(Registry& registry) {
    for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it) {
        it->system->OnDestroy(registry);
    }
    m_Systems.clear();
    m_TypeSet.clear();
}

} // namespace ECS
