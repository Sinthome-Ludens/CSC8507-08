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

    static void RegisterAll();
    static void Register(const std::string& name, EmplaceFn fn);
    static const EmplaceFn* Find(const std::string& name);

private:
    static std::unordered_map<std::string, EmplaceFn>& GetMap();
};

} // namespace ECS
