//
// Created by ZBN47MAX on 2026/2/24.
//
#pragma once

namespace ECS {
    struct C_D_AIPreception {
        float detectionValue = 0.0f;
        float detectionValueIncrease = 10.0f;
        float detectionValueDecrease = 5.0f;
        bool  isSpotted = false;
    };
}// namespace ECS
