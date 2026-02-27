#pragma once

namespace ECS {
    struct C_D_AIPreception {
        float detectionValue = 0.0f;
        float detectionValueIncrease = 15.0f;
        float detectionValueDecrease = 5.0f;
        bool  isSpotted = false;
        float huntLockTimer = 0.0f; // Hunt 状态锁定计时器
    };
}