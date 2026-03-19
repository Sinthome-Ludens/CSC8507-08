/**
 * @file Sys_Countdown.cpp
 * @brief 倒计时系统实现：管理关卡倒计时并在超时后触发 GameOver。
 *
 * @details
 * 每帧减少 `Res_GameState::countdownTimer`，到零时设置 `gameOverReason=1`。
 * 暂停时通过 PauseGuard 跳过更新。
 */
#include "Sys_Countdown.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Utils/Log.h"

#include <algorithm>

#ifdef USE_IMGUI
#include "Game/UI/UI_ActionNotify.h"
#endif

namespace ECS {

/**
 * @brief 每帧更新倒计时并判定超时。
 *
 * 当 `countdownActive` 为 true 时递减 `countdownTimer`，
 * 到零后设置 `isGameOver=true` + `gameOverReason=1`（倒计时耗尽）。
 */
void Sys_Countdown::OnUpdate(Registry& registry, float dt) {
    PAUSE_GUARD(registry);
    if (!registry.has_ctx<Res_GameState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;

    auto& gs = registry.ctx<Res_GameState>();
    auto& ui = registry.ctx<Res_UIState>();
    if (gs.isMultiplayer && gs.matchPhase != MatchPhase::Running) return;

    // Only run during HUD or None screen (gameplay)
    if (ui.activeScreen != UIScreen::HUD && ui.activeScreen != UIScreen::None) return;

    // Already game over — do nothing
    if (gs.isGameOver
        || (gs.isMultiplayer && gs.localTerminalState != MultiplayerTerminalState::None)) return;

    // Trigger: alertLevel reaches max and countdown not yet active
    if (gs.alertLevel >= gs.alertMax && !gs.countdownActive) {
        gs.countdownActive = true;
        gs.countdownTimer  = gs.countdownMax;
        LOG_INFO("[Sys_Countdown] Alert maxed — countdown started: " << gs.countdownMax << "s");
        // 倒计时积分惩罚 -200（挑战模式全局规则）
        if (!ui.countdownScorePenaltyApplied) {
            ui.countdownScorePenaltyApplied = true;
            ui.scoreLost_countdown += 200;
            ui.campaignScore = std::max(0, ui.campaignScore - 200);
#ifdef USE_IMGUI
            ECS::UI::PushActionNotify(registry, "ALERT SURGE", "COUNTDOWN",
                                      -200, ActionNotifyType::Alert);
#endif
        }
    }

    // Decrement countdown
    if (gs.countdownActive) {
        gs.countdownTimer -= dt;

        // Countdown expired
        if (gs.countdownTimer <= 0.0f) {
            gs.countdownTimer  = 0.0f;
            gs.countdownActive = false;
            if (gs.isMultiplayer) {
                gs.localTerminalState = MultiplayerTerminalState::Timeout;
                gs.localTerminalReason = 1u;
            } else {
                gs.isGameOver      = true;
                gs.gameOverReason  = 1;  // countdown expired
                gs.gameOverTime    = gs.playTime;
            }

            if (!gs.isMultiplayer) {
                ui.activeScreen = UIScreen::GameOver;
                ui.gameOverSelectedIndex = 0;
            }

            // 失败惩罚 -500（挑战模式全局规则）
            if (!ui.failureScorePenaltyApplied) {
                ui.failureScorePenaltyApplied = true;
                ui.scoreLost_failure += 500;
                ui.campaignScore = std::max(0, ui.campaignScore - 500);
#ifdef USE_IMGUI
                ECS::UI::PushActionNotify(registry, "MISSION", "FAILED",
                                          -500, ActionNotifyType::Alert);
#endif
            }

            LOG_INFO("[Sys_Countdown] Countdown expired — GameOver triggered");
        }
    }
}

} // namespace ECS
