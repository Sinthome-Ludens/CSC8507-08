/**
 * @file Res_RadarState.h
 * @brief 光子雷达全局状态资源（道具 002）。
 *
 * @details
 * 场景级 ctx 资源，由 Sys_ItemEffects 在玩家激活光子雷达时写入。
 * Sys_UI 读取此资源，在 HUD/小地图上绘制敌人位置点。
 *
 * ## 效果描述
 * 激活后在界面显示地图与敌人位置；敌人位置每 3 秒刷新一次。
 * 持续显示直到玩家手动关闭或场景卸载。
 *
 * @see Sys_ItemEffects.h
 * @see Sys_UI.h
 */
#pragma once

#include <cstdint>
#include "Vector.h"

namespace ECS {

/// @brief 单个检测到的敌人位置记录
struct RadarContact {
    NCL::Maths::Vector3 worldPos; ///< 敌人世界坐标（XZ 为水平面坐标）
    bool                valid = false; ///< 该记录是否有效
};

/**
 * @brief 光子雷达全局状态资源（Scene ctx）
 *
 * 由 Sys_ItemEffects 更新，由 Sys_UI 读取渲染。
 */
struct Res_RadarState {
    static constexpr int   kMaxContacts   = 16;   ///< 最大可追踪敌人数量
    static constexpr float kRefreshInterval = 3.0f; ///< 敌人位置刷新间隔（秒）

    bool  isActive          = false; ///< 雷达是否处于激活状态
    float refreshTimer      = 0.0f;  ///< 距下次刷新的剩余时间（秒）
    int   contactCount      = 0;     ///< 当前有效检测数量

    RadarContact contacts[kMaxContacts] = {}; ///< 检测到的敌人位置列表
};

} // namespace ECS
