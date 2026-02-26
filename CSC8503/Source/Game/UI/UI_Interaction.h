#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"

namespace ECS::UI {

/**
 * @brief 渲染世界空间交互提示（浮动在可交互实体上方）
 *
 * 遍历所有 C_D_Transform + C_D_Interactable 实体，
 * 筛选相机检测半径内的实体，将世界坐标投影到屏幕空间，
 * 在 ImGui ForegroundDrawList 上绘制赛博朋克风格的交互提示。
 *
 * @param registry ECS Registry（读取实体组件 + Res_NCL_Pointers context）
 * @param dt       帧间隔时间（秒），当前未使用，预留动画扩展
 */
void RenderInteractionPrompts(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
