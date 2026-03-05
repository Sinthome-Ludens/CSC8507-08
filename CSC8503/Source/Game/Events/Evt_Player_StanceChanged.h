#pragma once

#include "Core/ECS/EntityID.h"
#include "Game/Components/C_D_PlayerState.h"

/**
 * @brief 玩家姿态切换事件（延迟分发）
 *
 * 由 Sys_PlayerStance 在姿态发生变化时发布。
 * 监听者：动画系统（切换动画状态机）、音效系统（姿态切换音效）。
 */
struct Evt_Player_StanceChanged {
    ECS::EntityID    player;    ///< 玩家实体
    ECS::PlayerStance oldStance; ///< 切换前的姿态
    ECS::PlayerStance newStance; ///< 切换后的姿态
};
