#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"
#include "Game/Components/Res_UIState.h"

namespace ECS::UI {

// 共享导航辅助函数（Sys_UI ESC 和 Settings BACK 按钮共用）
void NavigateBackFromSettings(Res_UIState& ui);

void RenderSplashScreen  (Registry& registry, float dt);
void RenderMainMenu      (Registry& registry, float dt);
void RenderSettingsScreen(Registry& registry, float dt);
void RenderPauseMenu     (Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
