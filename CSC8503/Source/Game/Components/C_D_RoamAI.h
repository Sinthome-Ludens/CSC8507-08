/**
 * @file C_D_RoamAI.h
 * @brief 流窜 AI 数据组件：存储释放后流窜 AI 的巡逻状态（道具 004）。
 *
 * @details
 * 挂载在由"流窜 AI"道具释放的实体上，与 C_T_RoamAI 标签配合使用。
 * Sys_ItemEffects 读取此组件驱动随机游走，并检测与敌人的碰撞以触发消灭。
 *
 * ## 效果描述
 * 释放后在地图随机巡逻；触碰敌人时敌人死亡，流窜 AI 自身也随即销毁。
 *
 * @see C_T_RoamAI.h
 * @see Sys_ItemEffects.h
 */
#pragma once

#include "Vector.h"

namespace ECS {

/**
 * @brief 流窜 AI 巡逻状态数据组件
 */
struct C_D_RoamAI {
    NCL::Maths::Vector3 targetPos;          ///< 当前随机巡逻目标坐标
    float               roamSpeed  = 6.0f;  ///< 巡逻移动速度（m/s）
    float               waypointTimer = 0.0f; ///< 抵达目标后的停留计时器
    float               waypointInterval = 2.0f; ///< 随机换目标间隔（秒）
    float               detectRadius = 1.5f;  ///< 触碰敌人的检测半径（m）
    bool                active = true;        ///< 是否仍处于激活状态
};

} // namespace ECS
