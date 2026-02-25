#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

/// 渲染启动标题画面（CD机/唱片机风格）
void RenderTitleScreen(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
