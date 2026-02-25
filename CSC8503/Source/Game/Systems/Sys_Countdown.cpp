#include "Sys_Countdown.h"

#include "Game/Components/Res_GameplayState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_Countdown::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Res_GameplayState>()) return;
    if (!registry.has_ctx<Res_UIState>()) return;

    auto& gs = registry.ctx<Res_GameplayState>();
    auto& ui = registry.ctx<Res_UIState>();

    // 仅在游戏画面中运行（非菜单/GameOver）
    if (ui.activeScreen != UIScreen::HUD && ui.activeScreen != UIScreen::None) return;

    // 启动条件：alertLevel 达到上限 且 倒计时未激活
    if (!gs.countdownActive && gs.alertLevel >= gs.alertMax) {
        gs.countdownActive = true;
        gs.countdownTimer = gs.countdownMax;
        LOG_INFO("[Sys_Countdown] Countdown started! " << gs.countdownMax << "s");
    }

    // 倒计时递减
    if (gs.countdownActive) {
        gs.countdownTimer -= dt;

        if (gs.countdownTimer <= 0.0f) {
            gs.countdownTimer = 0.0f;
            gs.gameOverReason = 1;  // countdown_expired
            gs.gameOverTime = ui.globalTime;
            ui.gameOverSelectedIndex = 0;
            ui.activeScreen = UIScreen::GameOver;
            gs.countdownActive = false;
            LOG_INFO("[Sys_Countdown] Countdown expired! -> GameOver");
        }
    }
}

} // namespace ECS
