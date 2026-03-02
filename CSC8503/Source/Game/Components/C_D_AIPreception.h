#pragma once

namespace ECS {
    struct C_D_AIPreception {
        float detection_value = 0.0f;
        float detection_value_increase = 15.0f;
        float detection_value_decrease = 5.0f;
        bool  is_spotted = false;
        float hunt_lock_timer = 0.0f; // Hunt 状态锁定计时器
    };
}