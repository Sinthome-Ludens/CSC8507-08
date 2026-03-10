#include "Sys_PlayerDisguise.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_Hidden.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Utils/Log.h"

#include <cmath>

namespace ECS {

void Sys_PlayerDisguise::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Sys_Physics*>()) return;
    auto* physics = registry.ctx<Sys_Physics*>();
    if (!physics) return;

    registry.view<C_D_Input, C_D_RigidBody, C_T_Player, C_D_PlayerState>().each(
        [&](EntityID id, C_D_Input& input, C_D_RigidBody& rb,
            C_T_Player&, C_D_PlayerState& ps) {
            if (!rb.body_created) return;
            if (!input.disguiseJustPressed) return;

            if (ps.isDisguised) {
                // ── 退出伪装（任意时刻） ──
                ps.isDisguised = false;
                registry.Remove<C_T_Hidden>(id);
                LOG_INFO("[Sys_PlayerDisguise] Disguise OFF");
            } else {
                // ── 进入伪装条件检查 ──
                NCL::Maths::Vector3 vel = physics->GetLinearVelocity(id);
                float horizSpeed = std::sqrt(vel.x * vel.x + vel.z * vel.z);

                if (horizSpeed >= HIDE_SPEED_THRESHOLD) {
                    LOG_INFO("[Sys_PlayerDisguise] Cannot disguise: moving too fast");
                    return;
                }
                if (ps.stance == PlayerStance::Crouching) {
                    LOG_INFO("[Sys_PlayerDisguise] Cannot disguise: crouching");
                    return;
                }
                if (ps.wallState != WallState::None) {
                    LOG_INFO("[Sys_PlayerDisguise] Cannot disguise: wall state active");
                    return;
                }

                // ── 进入伪装 ──
                ps.isDisguised = true;

                // 若非 Standing 则请求强制站起（由 Sys_PlayerStance 执行）
                if (ps.stance != PlayerStance::Standing) {
                    ps.forceStandPending = true;
                }

                if (!registry.Has<C_T_Hidden>(id)) {
                    registry.Emplace<C_T_Hidden>(id);
                }
                LOG_INFO("[Sys_PlayerDisguise] Disguise ON");
            }
        }
    );
}

} // namespace ECS
