/**
 * @file Sys_GhostDuel.cpp
 * @brief Ghost Duel system: syncs ghost entity transform from Res_GhostDuelState.
 */
#include "Sys_GhostDuel.h"

#include "Game/Components/Res_GhostDuelState.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/C_T_GhostEntity.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Utils/Log.h"

namespace ECS {

void Sys_GhostDuel::OnAwake(Registry& /*reg*/) {
    LOG_INFO("[Sys_GhostDuel] OnAwake.");
}

void Sys_GhostDuel::OnUpdate(Registry& reg, float /*dt*/) {
    if (!reg.has_ctx<Res_GameState>()) return;
    const auto& gs = reg.ctx<Res_GameState>();
    if (!gs.isGhostDuel) return;

    if (!reg.has_ctx<Res_GhostDuelState>()) return;
    const auto& gd = reg.ctx<Res_GhostDuelState>();

    // Update ghost entity transform and visibility
    reg.view<C_T_GhostEntity, C_D_Transform>().each(
        [&](EntityID /*entity*/, C_T_GhostEntity&, C_D_Transform& tf) {
            if (gd.ghostVisible) {
                tf.position.x = gd.ghostPosX;
                tf.position.y = gd.ghostPosY;
                tf.position.z = gd.ghostPosZ;
                tf.rotation.x = gd.ghostRotX;
                tf.rotation.y = gd.ghostRotY;
                tf.rotation.z = gd.ghostRotZ;
                tf.rotation.w = gd.ghostRotW;
                tf.scale = NCL::Maths::Vector3(1.0f, 1.0f, 1.0f);
            } else {
                // Hide by moving far away (simple approach — no render enable/disable needed)
                tf.position.y = -9999.0f;
            }
        });
}

void Sys_GhostDuel::OnDestroy(Registry& /*reg*/) {
    LOG_INFO("[Sys_GhostDuel] OnDestroy.");
}

} // namespace ECS
