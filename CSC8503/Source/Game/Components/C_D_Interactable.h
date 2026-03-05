#pragma once

#include <cstdint>

namespace ECS {

enum class InteractableType : uint8_t {
    Generic = 0,
    Pickup,
    Door,
    Terminal,
    NPC,
};

struct C_D_Interactable {
    InteractableType type   = InteractableType::Generic;
    char  label[32]         = "INTERACT";
    float interactRange     = 3.0f;
    bool  isHighlighted     = false;
    bool  isEnabled         = true;
};

} // namespace ECS
