#include "Sys_StealthMetrics.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Events/Evt_Player_Noise.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"

#include <cmath>

using namespace NCL::Maths;

namespace ECS {

void Sys_StealthMetrics::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Sys_Physics*>()) return;
    auto* physics = registry.ctx<Sys_Physics*>();
    if (!physics) return;

    registry.view<C_D_Input, C_D_Transform, C_D_RigidBody, C_T_Player, C_D_PlayerState>().each(
        [&](EntityID id, C_D_Input& input, C_D_Transform& tf, C_D_RigidBody& rb,
            C_T_Player&, C_D_PlayerState& ps) {
            if (!rb.body_created) return;

            // 每实体噪音节流计时器递减（放在开头，只递减一次）
            ps.noiseCooldown -= dt;
            if (ps.noiseCooldown < 0.0f) ps.noiseCooldown = 0.0f;

            Vector3 vel = physics->GetLinearVelocity(id);
            float horizSpeed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
            bool isMoving = (horizSpeed > 0.1f);

            // ── 奔跑判定 + 速度乘数 ──
            float stanceMul = (ps.stance == PlayerStance::Crouching)
                              ? STANCE_MUL_CROUCHING : STANCE_MUL_STANDING;

            float disguiseMul = ps.isDisguised ? DISGUISE_MUL : 1.0f;
            ps.moveSpeedMul = stanceMul * disguiseMul;

            bool wantSprint = input.shiftDown && input.hasInput && !ps.isDisguised;

            if (wantSprint && ps.stance == PlayerStance::Crouching) {
                ps.forceStandPending = true;
                stanceMul = STANCE_MUL_STANDING;
                ps.moveSpeedMul = stanceMul * disguiseMul;
            }

            bool canSprint = wantSprint && ps.stance == PlayerStance::Standing;
            ps.isSprinting = canSprint;

            // ── 噪音/可见度指标 ──
            if (ps.isDisguised) {
                ps.visibilityFactor = isMoving ? 0.4f : 0.0f;
                ps.noiseLevel       = isMoving ? 0.5f : 0.0f;
            } else {
                switch (ps.stance) {
                    case PlayerStance::Standing:
                        ps.visibilityFactor = isMoving ? 1.0f : 0.7f;
                        ps.noiseLevel       = isMoving ? (ps.isSprinting ? 0.6f : 0.2f) : 0.0f;
                        break;
                    case PlayerStance::Crouching:
                        ps.visibilityFactor = isMoving ? 0.5f : 0.3f;
                        ps.noiseLevel       = isMoving ? (ps.isSprinting ? 0.6f : 0.2f) * 0.4f : 0.0f;
                        break;
                    default:
                        ps.visibilityFactor = 0.0f;
                        ps.noiseLevel       = 0.0f;
                        break;
                }
            }

            // ── 噪音事件发布（节流） ──
            if (isMoving && ps.noiseLevel >= 0.01f && ps.noiseCooldown <= 0.0f) {
                auto* bus = registry.has_ctx<EventBus*>() ? registry.ctx<EventBus*>() : nullptr;
                if (bus) {
                    Evt_Player_Noise evt{};
                    evt.source   = id;
                    evt.position = tf.position;
                    evt.volume   = ps.noiseLevel;
                    evt.type     = ps.isDisguised ? PlayerNoiseType::BoxScrape : PlayerNoiseType::Footstep;

                    bus->publish_deferred(evt);
                    ps.noiseCooldown = NOISE_THROTTLE;

                    LOG_INFO("[Sys_StealthMetrics] Noise: type="
                             << (int)evt.type << " vol=" << evt.volume);
                }
            }
        }
    );
}

} // namespace ECS