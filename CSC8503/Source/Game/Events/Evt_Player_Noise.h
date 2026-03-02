#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"

#include <cstdint>

/**
 * @brief 玩家噪音类型枚举
 */
enum class NoiseType : uint8_t {
    Footstep  = 0, ///< 脚步声
    WallKnock = 1, ///< 敲墙声（引诱守卫）
    BoxScrape = 2, ///< 纸箱拖动声
    Landing   = 3  ///< 落地声
};

/**
 * @brief 玩家噪音事件（延迟分发）
 *
 * 由 Sys_StealthMetrics 在玩家产生噪音时发布。
 * 监听者：AI 守卫系统（检测玩家位置）、Sys_Audio（音效播放）。
 */
struct Evt_Player_Noise {
    ECS::EntityID          source;   ///< 噪音源实体
    NCL::Maths::Vector3    position; ///< 噪音世界坐标
    float                  volume;   ///< 噪音音量 [0, 1]
    NoiseType              type;     ///< 噪音类型
};
