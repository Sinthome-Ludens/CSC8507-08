#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderItemWheel(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
