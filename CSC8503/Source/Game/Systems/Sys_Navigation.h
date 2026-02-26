#pragma once
#include "Core/ECS/BaseSystem.h"
#include "Game/Utils/PathfinderUtil.h" // 仅依赖接口

namespace ECS {
    class Sys_Navigation : public ISystem {
    public:
        Sys_Navigation() = default;

        // 通过接口注入寻路器（具体实现类会在外层完成注入）
        void SetPathfinder(PathfinderUtil* pf) { m_Pathfinder = pf; }

        void OnUpdate(Registry& registry, float dt) override;

    private:
        PathfinderUtil* m_Pathfinder = nullptr;
    };
}