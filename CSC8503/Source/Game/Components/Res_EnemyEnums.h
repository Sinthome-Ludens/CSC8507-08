//
// Created by ZBN47MAX on 2026/2/24.
//
#pragma once

namespace ECS {
    enum class EnemyState : uint8_t {
        Safe = 0,    // 巡逻/安全
        Caution = 1, // 警戒/搜索
        Alert = 2    // 发现玩家/追击
    };
}// namespace ECS
