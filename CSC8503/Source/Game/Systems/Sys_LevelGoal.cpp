/**
 * @file Sys_LevelGoal.cpp
 * @brief 关卡目标系统实现：每帧检测玩家与终点区域的距离，到达即胜利。
 */
#include "Sys_LevelGoal.h"
#include "Game/Utils/PauseGuard.h"

#include <algorithm>
#include <cmath>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_T_FinishZone.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Utils/Log.h"

#include "Game/Components/Res_ScoreConfig.h"
#include "Game/Components/Res_AudioConfig.h"
#include "Game/Events/Evt_Audio.h"
#include "Core/ECS/EventBus.h"
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
 * @details 单人模式下，当距离 < 4m 且高度差在阈值内时，设置
 *          Res_GameState::gameOverReason = 3（任务成功），切换 UI 到
 *          GameOver 画面并推送 Toast 通知，仅触发一次。
 *          多人模式下，每次到达终点会推进关卡进度；到达最终关卡终点时：
 *          - 若分数达标，则设置 gameOverReason = 3（战役成功结束）；
 *          - 若分数过低，则设置 gameOverReason = 2（分数不足导致的失败）。
 * @param registry ECS 注册表
 * @param dt       帧时间（未使用）
 */
void Sys_LevelGoal::OnUpdate(Registry& registry, float /*dt*/) {
    PAUSE_GUARD(registry);

    // 获取玩家位置
    NCL::Maths::Vector3 playerPos{};
    bool hasPlayer = false;
    registry.view<C_T_Player, C_D_Transform>().each(
        [&](EntityID, C_T_Player&, C_D_Transform& tf) {
            playerPos = tf.position;
            hasPlayer = true;
        });
    if (!hasPlayer) return;

    // 检测玩家与所有终点区域的 3D 距离（支持多层垂直地图）
    constexpr float kFinishRadiusXZ = 4.0f;   // XZ 平面触发半径
    constexpr float kFinishHeightMax = 3.0f;   // Y 最大高度差（防止上下层误触发）
    const bool isMultiplayer = registry.has_ctx<Res_GameState>()
        && registry.ctx<Res_GameState>().isMultiplayer;
    if (!isMultiplayer && m_FinishTriggered) return;

    registry.view<C_T_FinishZone, C_D_Transform>().each(
        [&](EntityID finishId, C_T_FinishZone&, C_D_Transform& ftf) {
            float dx = playerPos.x - ftf.position.x;
            float dy = playerPos.y - ftf.position.y;
            float dz = playerPos.z - ftf.position.z;
            float distXZSq = dx * dx + dz * dz;

            if (distXZSq < kFinishRadiusXZ * kFinishRadiusXZ
                && std::fabs(dy) < kFinishHeightMax) {
                if (isMultiplayer && m_FinishTriggered) {
                    return;
                }

                m_FinishTriggered = true;
                LOG_INFO("[Sys_LevelGoal] Player reached finish zone "
                         << (int)finishId << " distXZ=" << std::sqrt(distXZSq)
                         << " dY=" << dy);
                if (registry.has_ctx<EventBus*>()) {
                    auto* audioBus = registry.ctx<EventBus*>();
                    if (audioBus) audioBus->publish_deferred<Evt_Audio_PlaySFX>(Evt_Audio_PlaySFX{SfxId::FinishZone});
                }

                if (isMultiplayer) {
                    auto& gs = registry.ctx<Res_GameState>();
                    Res_ScoreConfig defaultScoreCfg;
                    const auto& scoreCfg = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg;
                    int32_t campaignScore = scoreCfg.initialScore;
#ifdef USE_IMGUI
                    Res_UIState* uiState = registry.has_ctx<Res_UIState>()
                        ? &registry.ctx<Res_UIState>()
                        : nullptr;
                    if (uiState != nullptr) {
                        campaignScore = uiState->campaignScore;
                    }
#endif
                    if (gs.localStageProgress < kMultiplayerStageCount) {
                        ++gs.localStageProgress;
                    }
                    gs.currentRoundIndex = std::min<uint8_t>(gs.localStageProgress, kMultiplayerStageCount - 1);
                    gs.localProgress = gs.localStageProgress;
                    gs.roundJustAdvanced = true;
                    if (gs.localStageProgress >= kMultiplayerStageCount) {
                        gs.isGameOver = true;
                        const bool scorePassed = campaignScore > scoreCfg.passThreshold;
                        gs.gameOverReason = scorePassed ? GameOverReason::Success : GameOverReason::Detected;
                        gs.gameOverTime = gs.playTime;
                        if (registry.has_ctx<Res_AudioState>()) {
                            registry.ctx<Res_AudioState>().requestedBgm = scorePassed ? BgmId::Victory : BgmId::Defeat;
                            registry.ctx<Res_AudioState>().bgmOverride  = false;
                        }
                    }

#ifdef USE_IMGUI
                    if (uiState != nullptr) {
                        auto& ui = *uiState;
                        if (gs.localStageProgress < kMultiplayerStageCount) {
                            ui.totalPlayTime += gs.playTime;
                            ui.pendingSceneRequest = SceneRequest::NextLevel;
                            UI::PushToast(registry, "AREA CLEAR - MOVING OUT", ToastType::Success, 2.0f);
                        } else {
                            const bool scorePassed = ui.campaignScore > scoreCfg.passThreshold;
                            UI::PushToast(registry,
                                          scorePassed ? "FINAL STAGE CLEAR" : "SCORE TOO LOW",
                                          scorePassed ? ToastType::Success : ToastType::Warning,
                                          2.0f);
                        }
                    }
#else
                    if (gs.localStageProgress >= kMultiplayerStageCount) {
                        gs.isGameOver = true;
                    }
#endif
                    return;
                }

#ifdef USE_IMGUI
                if (registry.has_ctx<Res_UIState>()) {
                    auto& ui = registry.ctx<Res_UIState>();

                    if (ui.debugCurrentScene >= 0) {
                        // ── Debug 模式：直接重启当前地图，不使用地图池 ──
                        if (registry.has_ctx<Res_GameState>()) {
                            registry.ctx<Res_GameState>().isGameOver = true;
                        }
                        ui.pendingSceneRequest = SceneRequest::RestartLevel;
                        UI::PushToast(registry, "AREA CLEAR - RESTARTING",
                                      ToastType::Success, 2.0f);
                        LOG_INFO("[Sys_LevelGoal] Debug mode -> RestartLevel (debugScene="
                                 << (int)ui.debugCurrentScene << ")");
                    } else if (ui.mapSequenceGenerated
                               && ui.mapSequenceIndex < Res_UIState::MAP_SEQUENCE_LENGTH - 1) {
                        // ── 非最终关：冻结游戏，累加时间，触发 NextLevel ──
                        if (registry.has_ctx<Res_GameState>()) {
                            auto& gs = registry.ctx<Res_GameState>();
                            gs.isGameOver = true;
                            ui.totalPlayTime += gs.playTime;
                        }
                        ui.pendingSceneRequest = SceneRequest::NextLevel;
                        UI::PushToast(registry, "AREA CLEAR - MOVING OUT",
                                      ToastType::Success, 2.0f);
                        LOG_INFO("[Sys_LevelGoal] Mid-sequence -> NextLevel (index="
                                 << (int)ui.mapSequenceIndex << ")");
                    } else {
                        // ── 最终关：累加时间，战役→Victory / 非战役→GameOver ──
                        Res_ScoreConfig defaultScoreCfg;
                        const auto& scFinal = registry.has_ctx<Res_ScoreConfig>() ? registry.ctx<Res_ScoreConfig>() : defaultScoreCfg;
                        if (registry.has_ctx<Res_GameState>()) {
                            auto& gs = registry.ctx<Res_GameState>();
                            gs.isGameOver     = true;
                            gs.gameOverReason = GameOverReason::Success;
                            ui.totalPlayTime += gs.playTime;
                        }
                        const bool finalPassed = ui.campaignScore > scFinal.passThreshold;
                        if (ui.mapSequenceGenerated && finalPassed) {
                            ui.activeScreen = UIScreen::Victory;
                            if (registry.has_ctx<Res_AudioState>()) {
                                registry.ctx<Res_AudioState>().requestedBgm = BgmId::Victory;
                                registry.ctx<Res_AudioState>().bgmOverride  = false;
                            }
                        } else {
                            ui.activeScreen = UIScreen::GameOver;
                            ui.gameOverSelectedIndex = 0;
                            if (registry.has_ctx<Res_AudioState>()) {
                                registry.ctx<Res_AudioState>().requestedBgm = BgmId::Defeat;
                                registry.ctx<Res_AudioState>().bgmOverride  = false;
                            }
                        }
                        UI::PushToast(registry,
                                      finalPassed ? "MISSION COMPLETE" : "MISSION FAILED",
                                      finalPassed ? ToastType::Success : ToastType::Warning,
                                      3.0f);
                    }
                }
#else
                if (registry.has_ctx<Res_GameState>()) {
                    auto& gs = registry.ctx<Res_GameState>();
                    gs.isGameOver     = true;
                    gs.gameOverReason = GameOverReason::Success;
                }
#endif
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
