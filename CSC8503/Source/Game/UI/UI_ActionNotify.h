/**
 * @file UI_ActionNotify.h
 * @brief 动作通知卡片渲染 + PushActionNotify 工具函数声明。
 *
 * @see UI_ActionNotify.cpp, Res_ActionNotifyState.h
 */
#pragma once
#ifdef USE_IMGUI

#include "Core/ECS/Registry.h"
#include "Game/Components/Res_ActionNotifyState.h"

namespace ECS::UI {

/**
 * @brief 向右上角推入一条动作通知（纯 UI 推送，不修改积分）。
 * @param registry   ECS 注册表
 * @param verb       动作动词（UTF-8），不可为 nullptr；空字符串表示不显示
 * @param target     目标名称（UTF-8），不可为 nullptr；空字符串表示不显示
 * @param scoreDelta 分值变化（0 表示不显示分值）
 * @param type       通知类型，决定目标名颜色
 * @param lifetime   显示时长（秒），默认 2.5s
 * @note verb 和 target 传 nullptr 时实现会静默替换为空字符串（防御性处理）
 */
void PushActionNotify(Registry& registry, const char* verb, const char* target,
                      int scoreDelta, ActionNotifyType type, float lifetime = 2.5f);

/**
 * @brief 每帧推进所有活跃通知的计时并渲染卡片。
 * @param registry ECS 注册表
 * @param dt       帧时间（秒）
 */
void RenderActionNotify(Registry& registry, float dt);

} // namespace ECS::UI

#endif // USE_IMGUI
