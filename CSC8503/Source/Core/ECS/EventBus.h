/**
 * @file EventBus.h
 * @brief ECS 事件总线：类型安全的发布/订阅与延迟事件队列
 *
 * @details
 * `EventBus` 提供了一个轻量级的**发布-订阅（Pub/Sub）**事件系统，使 System 之间
 * 能够解耦通信，无需直接相互引用。
 *
 * ## 核心概念
 *
 * - **事件（Event）**：任意可复制构造的 struct/class，建议命名以 `E_` 开头。
 * - **订阅者（Subscriber）**：注册回调函数 `void(const E&)` 的一方。
 * - **发布者（Publisher）**：调用 `publish<E>(event)` 的一方。
 * - **订阅句柄（SubscriptionID）**：`subscribe` 返回的整型 ID，用于后续取消订阅。
 *
 * ## 两种调度模式
 *
 * ### 即时调度（`publish<E>(event)`）
 * 调用时立即遍历所有已注册的 `E` 类型监听器并同步执行，适用于：
 * - 碰撞事件（OnCollision）
 * - 道具拾取（OnPickup）
 * - 任何需要当帧立即响应的场景
 *
 * ### 延迟调度（`publish_deferred<E>(event)`）
 * 将事件封装为类型擦除的 `std::function<void()>` 压入 `m_DeferredQueue`，
 * 等待显式调用 `flush()` 后才执行所有监听器，适用于：
 * - 跨 System 边界触发（避免在 A 系统迭代中直接触发 B 系统的副作用）
 * - 需要批量累积后统一处理的场景（如帧末网络事件、音效触发）
 *
 * ## 实现原理
 *
 * 监听器存储结构：
 * ```
 * m_Handlers : unordered_map<type_index, HandlerList>
 *                    │
 *                    └── HandlerList : vector<{ SubscriptionID, function<void(const void*)> }>
 * ```
 * 所有监听器统一存储为 `std::function<void(const void*)>`（类型擦除），
 * 内部通过 `reinterpret_cast<const E*>(data)` 还原为具体事件类型，再转发调用。
 *
 * `SubscriptionID` 由全局单调递增计数器 `m_NextID` 生成，保证全局唯一。
 *
 * ## 线程安全
 * EventBus **不是线程安全的**，所有操作应在同一线程（主逻辑线程）上调用。
 *
 * ## 使用示例
 * @code
 * // 定义事件
 * struct E_Phys_Collision {
 *     ECS::EntityID entityA;
 *     ECS::EntityID entityB;
 *     NCL::Maths::Vector3 contactPoint;
 * };
 *
 * // 订阅（OnAwake 中）
 * auto id = bus.subscribe<E_Phys_Collision>([](const E_Phys_Collision& e) {
 *     // 处理碰撞事件
 * });
 *
 * // 发布（立即）
 * bus.publish(E_Phys_Collision{ entityA, entityB, point });
 *
 * // 取消订阅
 * bus.unsubscribe<E_Phys_Collision>(id);
 *
 * // 延迟发布 + 帧末 flush
 * bus.publish_deferred(E_Phys_Collision{ entityA, entityB, point });
 * bus.flush(); // 在帧末或特定时机调用
 * @endcode
 *
 * @see BaseSystem.h
 * @see Registry.h
 */

#pragma once

#include "Game/Utils/Assert.h"
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <algorithm>

namespace ECS {

/// @brief 订阅句柄类型，由 subscribe() 返回，用于 unsubscribe() 取消注册。
using SubscriptionID = uint64_t;

/**
 * @brief 类型安全的事件总线，支持即时和延迟两种发布模式。
 *
 * EventBus 通常以单例或成员变量方式在 Scene 中持有，通过引用传递给各 System。
 */
class EventBus {
public:
    EventBus()  = default;
    ~EventBus() = default;

    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&)                 = default;
    EventBus& operator=(EventBus&&)      = default;

    /**
     * @brief 订阅 E 类型事件，返回可用于取消订阅的句柄 ID。
     * @details 内部将 `std::function<void(const E&)>` 包装为
     *          `std::function<void(const void*)>`（类型擦除），压入 `m_Handlers[typeid(E)]`。
     * @tparam E    事件类型，需可复制构造。
     * @param callback 事件触发时调用的回调函数，签名为 `void(const E&)`。
     * @return 全局唯一的 SubscriptionID，用于 `unsubscribe<E>(id)` 取消注册。
     */
    template<typename E>
    SubscriptionID subscribe(std::function<void(const E&)> callback);

    /**
     * @brief 取消对 E 类型事件的订阅。
     * @details 在 `m_Handlers[typeid(E)]` 中线性搜索目标 ID 并移除（swap-and-pop）。
     *          若 ID 不存在则为空操作（no-op）。
     * @tparam E  事件类型。
     * @param id  由 `subscribe<E>()` 返回的句柄 ID。
     */
    template<typename E>
    void unsubscribe(SubscriptionID id);

    /**
     * @brief 立即向所有 E 类型订阅者广播事件（同步执行）。
     * @details 将 `&event` 的地址以 `const void*` 传入各监听器，监听器内部
     *          通过 `reinterpret_cast<const E*>` 还原，再调用用户回调。
     * @tparam E    事件类型。
     * @param event 要广播的事件对象（const 引用，函数返回前不会析构）。
     */
    template<typename E>
    void publish(const E& event);

    /**
     * @brief 将事件推入延迟队列，在 `flush()` 被调用时统一执行。
     * @details 内部将事件**按值拷贝**到一个 lambda 捕获列表中，封装为
     *          `std::function<void()>` 压入 `m_DeferredQueue`。
     *          拷贝发生在 `publish_deferred` 调用时，之后原事件可安全销毁。
     * @tparam E    事件类型，需可复制构造（用于 lambda 捕获）。
     * @param event 要延迟广播的事件对象。
     */
    template<typename E>
    void publish_deferred(E event);

    /**
     * @brief 执行所有延迟队列中的事件（按加入顺序 FIFO 执行），然后清空队列。
     * @details 建议在每帧所有 System `OnUpdate` 完成后调用，也可在特定时机手动调用。
     *          flush 期间若有新的 `publish_deferred` 调用，它们会被追加到当前正在
     *          处理的队列之后（队列快照处理不会导致无限递归）。
     */
    void flush();

    /**
     * @brief 丢弃延迟队列中所有尚未执行的事件（不触发任何回调）。
     * @details 场景切换时调用，防止旧场景的事件在新场景中意外触发。
     */
    void clear_deferred();

    /**
     * @brief 清除所有事件类型的监听器和延迟队列，完全重置 EventBus。
     * @details 场景卸载时调用。
     */
    void clear();

private:
    /// @brief 类型擦除的监听器条目，存储句柄 ID 与类型擦除的回调函数。
    struct HandlerEntry {
        SubscriptionID             id;
        std::function<void(const void*)> callback;
    };

    using HandlerList = std::vector<HandlerEntry>;

    std::unordered_map<std::type_index, HandlerList> m_Handlers;     ///< 类型 → 监听器列表。
    std::vector<std::function<void()>>               m_DeferredQueue; ///< 待延迟执行的事件闭包队列。
    SubscriptionID                                   m_NextID = 1;    ///< 单调递增的 ID 计数器（从 1 开始，0 保留为无效值）。
};

// =============================================================================
// Template Implementation
// =============================================================================

template<typename E>
SubscriptionID EventBus::subscribe(std::function<void(const E&)> callback) {
    GAME_ASSERT(callback, "EventBus::subscribe - callback must not be null");

    SubscriptionID id = m_NextID++;
    auto key = std::type_index(typeid(E));

    m_Handlers[key].push_back({
        id,
        [cb = std::move(callback)](const void* data) {
            cb(*reinterpret_cast<const E*>(data));
        }
    });

    return id;
}

template<typename E>
void EventBus::unsubscribe(SubscriptionID id) {
    auto key = std::type_index(typeid(E));
    auto it  = m_Handlers.find(key);
    if (it == m_Handlers.end()) { return; }

    auto& list = it->second;
    auto  end  = std::remove_if(list.begin(), list.end(),
                                [id](const HandlerEntry& e) { return e.id == id; });
    list.erase(end, list.end());
}

template<typename E>
void EventBus::publish(const E& event) {
    auto key = std::type_index(typeid(E));
    auto it  = m_Handlers.find(key);
    if (it == m_Handlers.end()) { return; }

    const void* data = &event;
    for (const auto& entry : it->second) {
        entry.callback(data);
    }
}

template<typename E>
void EventBus::publish_deferred(E event) {
    auto key = std::type_index(typeid(E));

    m_DeferredQueue.emplace_back(
        [this, key, capturedEvent = std::move(event)]() {
            auto it = m_Handlers.find(key);
            if (it == m_Handlers.end()) { return; }
            const void* data = &capturedEvent;
            for (const auto& entry : it->second) {
                entry.callback(data);
            }
        }
    );
}

inline void EventBus::flush() {
    std::vector<std::function<void()>> snapshot;
    snapshot.swap(m_DeferredQueue);
    for (auto& fn : snapshot) {
        fn();
    }
}

inline void EventBus::clear_deferred() {
    m_DeferredQueue.clear();
}

inline void EventBus::clear() {
    m_Handlers.clear();
    m_DeferredQueue.clear();
    m_NextID = 1;
}

} // namespace ECS
