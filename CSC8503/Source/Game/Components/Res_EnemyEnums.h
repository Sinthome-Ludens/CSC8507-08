#pragma once
#include <cstdint>

namespace ECS {
    enum class EnemyState : uint8_t {
        Safe   = 0, // [0, 20)
        Search = 1, // [20, 40)
        Alert  = 2, // [40, 80)
        Hunt   = 3  // [80, 100]
    };
}