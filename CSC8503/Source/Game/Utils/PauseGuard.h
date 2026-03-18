/**
 * @file PauseGuard.h
 * @brief PAUSE_GUARD macro — early-return from OnUpdate when the game is paused.
 *
 * Usage: place `PAUSE_GUARD(registry);` as the first line of OnUpdate.
 * Scenes without Res_GameState (e.g. NavTest) are unaffected.
 */
#pragma once
#include "Game/Components/Res_GameState.h"

#define PAUSE_GUARD(registry)                                       \
    do {                                                            \
        if ((registry).has_ctx<ECS::Res_GameState>() &&             \
            (registry).ctx<ECS::Res_GameState>().isPaused) return;  \
    } while(0)
