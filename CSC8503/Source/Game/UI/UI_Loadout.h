#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderLoadoutScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
