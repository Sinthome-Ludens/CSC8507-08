#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderScanlineOverlay(float globalTime);
void RenderTransitionOverlay(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
