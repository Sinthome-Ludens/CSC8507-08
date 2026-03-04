#pragma once

#include "Core/ECS/BaseSystem.h"
#include <cstdint>

namespace ECS {

class Sys_Raycast : public ISystem {
public:
    void OnAwake(Registry& registry) override;
    void OnUpdate(Registry& registry, float dt) override;
    void OnDestroy(Registry& registry) override;

private:
    struct RaycastResult {
        bool hit = false;
        float distance = 0.0f;
        float maxDistance = 60.0f;

        float originX = 0.0f;
        float originY = 0.0f;
        float originZ = 0.0f;

        float endX = 0.0f;
        float endY = 0.0f;
        float endZ = 0.0f;

        float hitX = 0.0f;
        float hitY = 0.0f;
        float hitZ = 0.0f;

        uint32_t bodyID = 0xFFFFFFFF;
        uint32_t entityID = 0xFFFFFFFF;
    };

    bool m_ShowWindow = true;
    bool m_EnableRaycast = true;
    bool m_ShowRay = true;

    RaycastResult m_LastResult;
};

} // namespace ECS
