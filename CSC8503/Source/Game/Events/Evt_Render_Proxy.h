#pragma once
#include "Core/ECS/Registry.h"

/// @brief 渲染代理创建事件：ECS 实体第一次出现在渲染系统中时触发
struct Evt_Render_ProxyCreated {
    ECS::EntityID entity; ///< 对应的 ECS 实体 ID
};

/// @brief 渲染代理销毁事件：ECS 实体从渲染系统中移除时触发
struct Evt_Render_ProxyDestroyed {
    ECS::EntityID entity; ///< 对应的 ECS 实体 ID
};
