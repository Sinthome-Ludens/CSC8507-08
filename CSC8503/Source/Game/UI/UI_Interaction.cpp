#include "UI_Interaction.h"
#ifdef USE_IMGUI

#include <imgui.h>
#include "Game/Components/Res_UIState.h"
#include "Game/UI/UITheme.h"

namespace ECS::UI {

// ============================================================
// RenderInteractionPrompts — World-space floating labels
// ============================================================
// 占位实现：实际的世界坐标→屏幕投影需要相机矩阵，
// 待集成渲染系统后由调用方提供屏幕坐标。

void RenderInteractionPrompts(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Res_UIState>()) return;
    // 当前无可交互实体的屏幕坐标数据，暂不渲染。
}

} // namespace ECS::UI

#endif // USE_IMGUI
