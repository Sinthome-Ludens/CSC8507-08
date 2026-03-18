/**
 * @file ComponentRegistry.h
 * @brief 组件反射注册表：组件名 → Emplace 函数的映射。
 *
 * @details
 * 提供 JSON 组件名到 Emplace lambda 的运行时映射，供 PrefabFactory::Create() 通用入口使用。
 * RegisterAll() 幂等注册全部已知组件类型。
 */
#pragma once

#include "Core/ECS/Registry.h"
#include "RuntimeOverrides.h"

#include <nlohmann/json_fwd.hpp>
#include <functional>
#include <string>
#include <unordered_map>

namespace ECS {

using EmplaceFn = std::function<void(Registry&, EntityID, const nlohmann::json&, const RuntimeOverrides&)>;

class ComponentRegistry {
public:
    ComponentRegistry()  = delete;
    ~ComponentRegistry() = delete;

    /**
     * @brief 幂等注册全部已知组件类型。
     *
     * 内部使用 static bool 保证只执行一次。首次调用时注册约 32 个组件的
     * Emplace lambda 到内部映射表。后续调用立即返回，无开销。
     * 非线程安全（仅在主线程调用）。
     */
    static void RegisterAll();

    /**
     * @brief 注册或覆盖一个组件名到 Emplace 函数的映射。
     *
     * 若 name 已存在，新的 fn 会覆盖旧值。
     *
     * @param name 组件名（如 "C_D_Transform"），须与 JSON 蓝图中的 key 一致
     * @param fn   Emplace 函数，接收 (Registry&, EntityID, json&, RuntimeOverrides&)
     */
    static void Register(const std::string& name, EmplaceFn fn);

    /**
     * @brief 按组件名查找已注册的 Emplace 函数。
     *
     * 返回的指针指向内部 static map 中的元素，生命周期为进程级别。
     * 在 RegisterAll() 完成后不会再修改 map，因此指针稳定。
     *
     * @param name 组件名
     * @return 指向 EmplaceFn 的 const 指针，未找到时返回 nullptr
     */
    static const EmplaceFn* Find(const std::string& name);

private:
    /** @brief Meyer's singleton：返回内部 static map 的引用。 */
    static std::unordered_map<std::string, EmplaceFn>& GetMap();
};

} // namespace ECS
