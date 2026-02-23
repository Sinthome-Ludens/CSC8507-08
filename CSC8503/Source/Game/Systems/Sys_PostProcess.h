#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Core/Bridge/PostProcessPipeline.h"

namespace ECS {

class Sys_PostProcess : public ISystem {
public:
    void OnAwake(Registry& registry)                override;
    void OnLateUpdate(Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)              override;

private:
    PostProcessPipeline m_Pipeline;
    bool m_Initialized = false;
    int  m_LastWidth   = 0;
    int  m_LastHeight  = 0;
};

} // namespace ECS
