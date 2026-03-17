/**
 * @file C_D_AIPerception.h
 * @brief AI 感知数据组件：警戒度累积、状态阈值与 Hunt 锁定计时器。
 *
 * @details
 * 由 Sys_EnemyVision 写入 is_spotted 和 spotted_distance，
 * 由 Sys_EnemyAI 读取阈值（hunt/alert/search_threshold）以驱动状态切换。
 * spotted_distance 用于距离调制：远处的瞥见警戒累积更慢。
 */
#pragma once

namespace ECS {
    struct C_D_AIPerception {
        float detection_value          = 0.0f;
        float detection_value_increase = 8.0f;
        float detection_value_decrease = 15.0f;
        bool  is_spotted               = false;
        float hunt_lock_timer          = 0.0f;

        float hunt_threshold   = 80.0f;
        float alert_threshold  = 40.0f;
        float search_threshold = 20.0f;

        float spotted_distance = 0.0f;
    };
}
