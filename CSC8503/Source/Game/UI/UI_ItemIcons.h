/**
 * @file UI_ItemIcons.h
 * @brief 5 种道具的几何图标绘制（ImDrawList 原语，赛博线框风）。
 */
#pragma once

#ifdef USE_IMGUI
#include <imgui.h>
#include "Game/Components/C_D_Item.h"

namespace ECS::UI {

/// @brief 绘制道具几何图标。
/// @param draw  ImDrawList 绘制目标
/// @param center 图标中心坐标
/// @param size  图标尺寸（半径）
/// @param id    道具 ID
/// @param color 线条颜色
void DrawItemIcon(ImDrawList* draw, ImVec2 center, float size, ItemID id, ImU32 color);

} // namespace ECS::UI

#endif // USE_IMGUI
