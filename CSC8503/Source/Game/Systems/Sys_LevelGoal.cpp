/**
 * @file Sys_LevelGoal.cpp
 * @brief 关卡目标系统实现：每帧检测玩家与终点区域的距离，到达即胜利。
 */
#include "Sys_LevelGoal.h"
#include "Game/Utils/PauseGuard.h"

#include <cmath>
#include <cstdio>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_T_FinishZone.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/Res_CampaignState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_ToastState.h"
#include "Game/UI/UI_Toast.h"
#endif

namespace ECS {

/**
 * @brief 初始化：重置触发标志。
 * @param registry ECS 注册表（未使用）
 */
void Sys_LevelGoal::OnAwake(Registry& /*registry*/) {
    m_FinishTriggered = false;
    LOG_INFO("[Sys_LevelGoal] OnAwake");
}

/**
 * @brief 每帧检测玩家与所有 C_T_FinishZone 实体的 XZ 距离。
 * @details 当距离 < 4m 时设置 Res_GameState::gameOverReason = 3（任务成功），
 *          并切换 UI 到 GameOver 画面、推送 Toast 通知。仅触发一次。
 * @param registry ECS 注册表
 * @param dt       帧时间（未使用）
 */
void Sys_LevelGoal::OnUpdate(Registry& registry, float /*dt*/) {
    PAUSE_GUARD(registry);
    if (m_FinishTriggered) return;

    // 获取玩家位置
    NCL::Maths::Vector3 playerPos{};
    bool hasPlayer = false;
    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID, C_T_Player&, C_D_Transform& tf) {
            playerPos = tf.position;
            hasPlayer = true;
        });
    if (!hasPlayer) return;

    // 检测玩家与所有终点区域的 XZ 距离
    constexpr float kFinishRadiusSq = 4.0f * 4.0f;  // 4m 触发半径

    registry.view<C_T_FinishZone, C_D_Transform>().each(
        [&](EntityID finishId, C_T_FinishZone&, C_D_Transform& ftf) {
            if (m_FinishTriggered) return;

            float dx = playerPos.x - ftf.position.x;
            float dz = playerPos.z - ftf.position.z;
            float distSq = dx * dx + dz * dz;

            if (distSq < kFinishRadiusSq) {
                m_FinishTriggered = true;
                LOG_INFO("[Sys_LevelGoal] Player reached finish zone "
                         << (int)finishId << " dist=" << std::sqrt(distSq));

                // === Campaign mode branch ===
                bool isCampaign = registry.has_ctx<Res_CampaignState>()
                               && registry.ctx<Res_CampaignState>().active;

                if (isCampaign) {
                    auto& campaign = registry.ctx<Res_CampaignState>();

                    // Accumulate play time for this round
                    if (registry.has_ctx<Res_GameState>()) {
                        campaign.totalPlayTime += registry.ctx<Res_GameState>().playTime;
                    }

                    if (campaign.currentRound < kCampaignRounds - 1) {
                        // More maps remaining → auto-advance
                        // NOTE: increment currentRound BEFORE setting NextLevel so that
                        // Main.cpp reads mapSequence[currentRound] as the *next* map index.
                        campaign.currentRound++;
                        if (registry.has_ctx<Res_GameState>())
                            registry.ctx<Res_GameState>().isPaused = true;
#ifdef USE_IMGUI
                        if (registry.has_ctx<Res_UIState>()) {
                            registry.ctx<Res_UIState>().pendingSceneRequest = SceneRequest::NextLevel;
                        }
                        char msg[64];
                        snprintf(msg, sizeof(msg), "MAP %d/%d COMPLETE",
                                 campaign.currentRound, kCampaignRounds);
                        UI::PushToast(registry, msg, ToastType::Success, 2.0f);
#endif
                        LOG_INFO("[Sys_LevelGoal] Campaign round " << (int)campaign.currentRound
                                 << "/" << kCampaignRounds << " advancing to next map");
                    } else {
                        // Final map → victory
                        if (registry.has_ctx<Res_GameState>()) {
                            auto& gs = registry.ctx<Res_GameState>();
                            gs.isGameOver     = true;
                            gs.gameOverReason = 3;
                        }
#ifdef USE_IMGUI
                        if (registry.has_ctx<Res_UIState>())
                            registry.ctx<Res_UIState>().activeScreen = UIScreen::Victory;
                        UI::PushToast(registry, "CAMPAIGN COMPLETE", ToastType::Success, 3.0f);
#endif
                        LOG_INFO("[Sys_LevelGoal] Campaign complete!");
                    }
                } else {
                    // === Non-campaign (Tutorial etc.): original behavior ===
                    if (registry.has_ctx<Res_GameState>()) {
                        auto& gs = registry.ctx<Res_GameState>();
                        gs.isGameOver     = true;
                        gs.gameOverReason = 3;
                    }
#ifdef USE_IMGUI
                    if (registry.has_ctx<Res_UIState>()) {
                        registry.ctx<Res_UIState>().activeScreen = UIScreen::GameOver;
                    }
                    UI::PushToast(registry, "MISSION COMPLETE", ToastType::Success, 3.0f);
#endif
                }
            }
        });
}

/**
 * @brief 系统销毁：无需额外清理。
 * @param registry ECS 注册表（未使用）
 */
void Sys_LevelGoal::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_LevelGoal] OnDestroy");
}

} // namespace ECS
