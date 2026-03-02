//
// Created by ZBN47MAX on 2026/2/24.
//
#pragma once
#include "Game/Components/Res_EnemyEnums.h"
namespace ECS {
    // 文档规范：AI状态数据组件，存储敌人的当前AI有限状态机状态，仅包含数据不含逻辑
    struct C_D_AIState {
        EnemyState currentState = EnemyState::Safe;
    };
}// namespace ECS
