/**
 * @file C_D_CQCState.h
 * @brief 玩家 CQC 交互状态组件（POD）。
 */
#pragma once

#include "Core/ECS/EntityID.h"
#include <cstdint>

namespace ECS {

enum class CQCPhase : uint8_t {
    None     = 0,
    Approach = 1,
    Execute  = 2,
    Complete = 3
};

/// @brief Player CQC interaction state (POD)
struct C_D_CQCState {
    CQCPhase  phase         = CQCPhase::None;
    EntityID  targetEnemy   = 0;
    float     phaseTimer    = 0.0f;
    float     cooldown      = 0.0f;

    /* 目标选择 */
    EntityID  highlightedEnemy = 0;
    int       selectedIndex    = 0;
    int       candidateCount   = 0;
};

} // namespace ECS
