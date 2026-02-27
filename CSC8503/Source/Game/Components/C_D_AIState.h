//
// Created by ZBN47MAX on 2026/2/24.
//
#pragma once
#include "Game/Components/Res_EnemyEnums.h"
namespace ECS {
    // 文档规范：身份标签组件，仅作标识，不含逻辑
    struct C_D_AIState {
        EnemyState currentState = EnemyState::Safe;
    };
}// namespace ECS
