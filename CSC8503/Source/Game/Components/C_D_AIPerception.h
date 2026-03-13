/**
 * @file C_D_AIPerception.h
 * @brief 敌人 AI 感知数据组件。
 *
 * @details
 * 由 Sys_EnemyAI 读取阈值（hunt/alert/search_threshold）以驱动状态切换。
 */
#pragma once

namespace ECS {
    struct C_D_AIPerception {
        float detection_value          = 0.0f;
        float detection_value_increase = 15.0f;
        float detection_value_decrease = 5.0f;
        bool  is_spotted               = false;
        float hunt_lock_timer          = 0.0f; // Hunt 状态锁定计时器

        // 状态阈值（可在运行时调节，解耦 Sys_EnemyAI 中的硬编码值）
        float hunt_threshold    = 50.0f;  // 进入 Hunt 状态的警戒度下限
        float alert_threshold   = 30.0f;  // 进入 Alert 状态的警戒度下限
        float search_threshold  = 15.0f;  // 进入 Search 状态的警戒度下限
    };
}
