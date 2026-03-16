#include "Sys_Countdown.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_Countdown::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
    if (!registry.has_ctx<Res_GameState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;

    auto& gs = registry.ctx<Res_GameState>();
    auto& ui = registry.ctx<Res_UIState>();

    // Only run during HUD or None screen (gameplay)
    if (ui.activeScreen != UIScreen::HUD && ui.activeScreen != UIScreen::None) return;

    // Already game over — do nothing
    if (gs.isGameOver) return;

    // Trigger: alertLevel reaches max and countdown not yet active
    if (gs.alertLevel >= gs.alertMax && !gs.countdownActive) {
        gs.countdownActive = true;
        gs.countdownTimer  = gs.countdownMax;
        LOG_INFO("[Sys_Countdown] Alert maxed — countdown started: " << gs.countdownMax << "s");
    }

    // Decrement countdown
    if (gs.countdownActive) {
        gs.countdownTimer -= dt;

        // Countdown expired
        if (gs.countdownTimer <= 0.0f) {
            gs.countdownTimer  = 0.0f;
            gs.countdownActive = false;
            gs.isGameOver      = true;
            gs.gameOverReason  = 1;  // countdown expired
            gs.gameOverTime    = gs.playTime;

            ui.activeScreen         = UIScreen::GameOver;
            ui.gameOverSelectedIndex = 0;

            LOG_INFO("[Sys_Countdown] Countdown expired — GameOver triggered");
        }
    }
}

} // namespace ECS
