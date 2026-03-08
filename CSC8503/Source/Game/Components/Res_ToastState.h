/**
 * @file Res_ToastState.h
 * @brief Toast 通知状态资源：最多 4 条浮动提示（Info/Success/Warning/Danger）
 *
 * @details
 * Scene 级 ctx 资源，场景切换时在 OnExit 中清除。
 */
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
