#pragma once
#include <cstdint>

namespace ECS {
    enum class EnemyState : uint8_t {
        Safe    = 0, // [0, 15)
        Caution = 1, // [15, 30)
        Alert   = 2, // [30, 50)
        Hunt    = 3  // [50, 100]
    };
}