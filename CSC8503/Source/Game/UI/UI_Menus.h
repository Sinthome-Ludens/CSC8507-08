#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

void RenderSplashScreen  (Registry& registry, float dt);
void RenderMainMenu      (Registry& registry, float dt);
void RenderSettingsScreen(Registry& registry, float dt);
void RenderPauseMenu     (Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
