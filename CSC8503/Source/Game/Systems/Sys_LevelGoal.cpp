/**
 * @file Sys_LevelGoal.cpp
 * @brief 关卡目标系统实现：每帧检测玩家与终点区域的距离，到达即胜利。
 */
#include "Sys_LevelGoal.h"
#include "Game/Utils/PauseGuard.h"

#include <cmath>
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_T_FinishZone.h"
#include "Game/Components/C_T_Player.h"
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

    // 检测玩家与所有终点区域的 3D 距离（支持多层垂直地图）
    constexpr float kFinishRadiusXZ = 4.0f;   // XZ 平面触发半径
    constexpr float kFinishHeightMax = 3.0f;   // Y 最大高度差（防止上下层误触发）

    registry.view<C_T_FinishZone, C_D_Transform>().each(
        [&](EntityID finishId, C_T_FinishZone&, C_D_Transform& ftf) {
            if (m_FinishTriggered) return;

            float dx = playerPos.x - ftf.position.x;
            float dy = playerPos.y - ftf.position.y;
            float dz = playerPos.z - ftf.position.z;
            float distXZSq = dx * dx + dz * dz;

            if (distXZSq < kFinishRadiusXZ * kFinishRadiusXZ
                && std::fabs(dy) < kFinishHeightMax) {
                m_FinishTriggered = true;
                LOG_INFO("[Sys_LevelGoal] Player reached finish zone "
                         << (int)finishId << " distXZ=" << std::sqrt(distXZSq)
                         << " dY=" << dy);

#ifdef USE_IMGUI
                if (registry.has_ctx<Res_UIState>()) {
                    auto& ui = registry.ctx<Res_UIState>();

                    // 判断是否为序列中的最后一张地图
                    bool isFinalMap = !ui.mapSequenceGenerated
                                   || ui.mapSequenceIndex >= Res_UIState::MAP_SEQUENCE_LENGTH - 1;

                    if (!isFinalMap) {
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
                        if (registry.has_ctx<Res_GameState>()) {
                            auto& gs = registry.ctx<Res_GameState>();
                            gs.isGameOver     = true;
                            gs.gameOverReason = 3;
                            ui.totalPlayTime += gs.playTime;
                        }
                        if (ui.mapSequenceGenerated) {
                            ui.activeScreen = UIScreen::Victory;
                        } else {
                            ui.activeScreen = UIScreen::GameOver;
                        }
                        UI::PushToast(registry, "MISSION COMPLETE",
                                      ToastType::Success, 3.0f);
                    }
                }
#else
                if (registry.has_ctx<Res_GameState>()) {
                    auto& gs = registry.ctx<Res_GameState>();
                    gs.isGameOver     = true;
                    gs.gameOverReason = 3;
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
