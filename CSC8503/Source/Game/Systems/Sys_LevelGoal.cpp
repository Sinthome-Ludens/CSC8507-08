/**
 * @file Sys_LevelGoal.cpp
 * @brief 关卡目标系统实现：每帧检测玩家与终点区域的距离，到达即胜利。
 */
#include "Sys_LevelGoal.h"

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

void Sys_LevelGoal::OnAwake(Registry& /*registry*/) {
    m_FinishTriggered = false;
    LOG_INFO("[Sys_LevelGoal] OnAwake");
}

void Sys_LevelGoal::OnUpdate(Registry& registry, float /*dt*/) {
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

                if (registry.has_ctx<Res_GameState>()) {
                    auto& gs = registry.ctx<Res_GameState>();
                    gs.isGameOver     = true;
                    gs.gameOverReason = 3;  // 任务成功
                }

#ifdef USE_IMGUI
                if (registry.has_ctx<Res_UIState>()) {
                    auto& ui = registry.ctx<Res_UIState>();
                    ui.activeScreen = UIScreen::GameOver;
                }
                UI::PushToast(registry, "MISSION COMPLETE", ToastType::Success, 3.0f);
#endif
            }
        });
}

void Sys_LevelGoal::OnDestroy(Registry& /*registry*/) {
    LOG_INFO("[Sys_LevelGoal] OnDestroy");
}

} // namespace ECS
