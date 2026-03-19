/**
 * @file UI_Anim.h
 * @brief Lightweight animation utilities for UI (header-only).
 *
 * Easing functions, frame-rate-independent interpolation, pulse alpha,
 * and screen entry state for transition animations.
 */
#pragma once
#ifdef USE_IMGUI

#include <cmath>
#include <algorithm>

namespace ECS::UI::Anim {

// ============================================================
// Easing functions (t in [0,1] → result in [0,1])
// ============================================================

inline float EaseOutCubic(float t) {
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}

inline float EaseOutBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    float f = t - 1.0f;
    return 1.0f + c3 * f * f * f + c1 * f * f;
}

inline float EaseInOutQuad(float t) {
    return (t < 0.5f)
        ? 2.0f * t * t
        : 1.0f - 0.5f * ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f));
}

// ============================================================
// Frame-rate independent smooth interpolation
// ============================================================

/// @brief Smoothly interpolate current → target. speed ≈ 1/timeConstant.
inline float SmoothLerp(float current, float target, float speed, float dt) {
    float factor = 1.0f - std::exp(-speed * dt);
    return current + (target - current) * factor;
}

// ============================================================
// Pulse alpha (for breathing/blinking effects)
// ============================================================

/// @brief Returns a pulsing alpha value oscillating between lo and hi.
inline float PulseAlpha(float globalTime, float freq, float lo, float hi) {
    float t = (std::sin(globalTime * freq * 6.2831853f) + 1.0f) * 0.5f;
    return lo + t * (hi - lo);
}

// ============================================================
// Screen entry state (for page transition animations)
// ============================================================

struct ScreenEntryState {
    float elapsed  = 0.0f;
    float duration = 0.0f;

    /// Start a new entry animation.
    void Start(float dur = 0.35f) {
        elapsed  = 0.0f;
        duration = dur;
    }

    /// Tick the animation forward. Call once per frame.
    void Tick(float dt) {
        if (duration > 0.0f)
            elapsed = std::min(elapsed + dt, duration);
    }

    /// Returns animation progress [0,1]. 1.0 = fully entered (default/no anim).
    float Progress() const {
        if (duration <= 0.0f) return 1.0f;
        return std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    /// True if animation is still playing.
    bool IsPlaying() const {
        return duration > 0.0f && elapsed < duration;
    }
};

// ============================================================
// Screen depth (for slide transition direction)
// ============================================================

/// @brief Returns "depth" of a screen for transition direction.
///        Higher depth = deeper in menu hierarchy.
inline int ScreenDepth(ECS::UIScreen s) {
    switch (s) {
        case ECS::UIScreen::TitleScreen:  return 0;
        case ECS::UIScreen::Splash:       return 1;
        case ECS::UIScreen::MainMenu:     return 2;
        case ECS::UIScreen::Settings:     return 3;
        case ECS::UIScreen::MissionSelect:return 3;
        case ECS::UIScreen::Lobby:        return 3;
        case ECS::UIScreen::Team:         return 3;
        case ECS::UIScreen::PauseMenu:    return 2;
        case ECS::UIScreen::HUD:          return 1;
        case ECS::UIScreen::GameOver:     return 1;
        case ECS::UIScreen::Victory:      return 1;
        case ECS::UIScreen::Inventory:    return 3;
        case ECS::UIScreen::Loading:      return 1;
        default: return 0;
    }
}

/// @brief Compute horizontal slide offset for screen transition.
/// @param entryT  Entry animation progress [0,1] (1 = fully entered)
/// @param direction  1=forward (slide from right), -1=backward (slide from left)
/// @param maxSlide  Maximum slide distance in pixels (default 60)
inline float SlideOffset(float entryT, int8_t direction, float maxSlide = 60.0f) {
    return (float)direction * maxSlide * (1.0f - entryT);
}

} // namespace ECS::UI::Anim

#endif // USE_IMGUI
