#pragma once

#include <cstdint>

namespace ECS {

enum class ToastType : uint8_t {
    Info = 0,
    Success,
    Warning,
    Danger,
};

struct ToastEntry {
    char      text[64]  = {};
    float     lifetime  = 3.0f;
    float     elapsed   = 0.0f;
    ToastType type      = ToastType::Info;
    bool      active    = false;
};

struct Res_ToastState {
    static constexpr int kMaxToasts = 4;
    ToastEntry toasts[kMaxToasts] = {};
    int        nextSlot           = 0;
};

} // namespace ECS
