#pragma once

#include "Core/ECS/EntityID.h"
#include "Vector.h"

#include <cstdint>

namespace ECS {

/**
 * @brief 玩家噪音类型枚举
 * 作用域限定在 ECS 命名空间内，避免与 AI/Audio 等模块的同名类型冲突。
 */
enum class PlayerNoiseType : uint8_t {
    Footstep  = 0,
    WallKnock = 1,
    BoxScrape = 2,
    Landing   = 3
};

/**
 * @brief 玩家噪音事件（延迟分发）
 *
 * 由 Sys_StealthMetrics 在玩家产生噪音时发布。
 * 监听者：AI 守卫系统（检测玩家位置）、Sys_Audio（音效播放）。
 */
struct Evt_Player_Noise {
    EntityID               source;
    NCL::Maths::Vector3    position;
    float                  volume;
    PlayerNoiseType        type;
};

} // namespace ECS
