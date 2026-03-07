/**
 * @file C_D_Interactable.h
 * @brief 可交互组件：标记实体为可交互对象并存储交互参数
 */
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
