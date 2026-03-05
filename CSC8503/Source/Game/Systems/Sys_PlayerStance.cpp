#include "Sys_PlayerStance.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Physics.h"
#include "Game/Events/Evt_Player_StanceChanged.h"
#include "Game/Utils/Log.h"
#include "Core/ECS/EventBus.h"

namespace ECS {

void Sys_PlayerStance::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Sys_Physics*>()) return;
    auto* physics = registry.ctx<Sys_Physics*>();
    if (!physics) return;

    registry.view<C_D_Input, C_D_Transform, C_D_RigidBody, C_T_Player, C_D_PlayerState>().each(
        [&](EntityID id, C_D_Input& input, C_D_Transform& tf, C_D_RigidBody& rb,
            C_T_Player&, C_D_PlayerState& ps) {
            if (!rb.body_created) return;

            // 伪装状态下禁止手动姿态切换
            if (ps.isDisguised) {
                // 但仍需处理 forceStandPending（伪装进入时强制站起）
                if (ps.forceStandPending && ps.stance != PlayerStance::Standing) {
                    PlayerStance oldStance = ps.stance;
                    float oldHalfHeight = ps.colliderHalfHeight;

                    ps.stance = PlayerStance::Standing;
                    ps.colliderHalfHeight = STAND_HALF_HEIGHT;
                    physics->ReplaceShapeCapsule(rb.jolt_body_id, CAPSULE_RADIUS, STAND_HALF_HEIGHT);

                    float oldBottom = tf.position.y - (oldHalfHeight + CAPSULE_RADIUS);
                    float newCenterY = oldBottom + STAND_HALF_HEIGHT + CAPSULE_RADIUS + 0.05f;
                    physics->SetPosition(rb.jolt_body_id, tf.position.x, newCenterY, tf.position.z);
                    tf.position.y = newCenterY;
                    physics->ActivateBody(rb.jolt_body_id);

                    auto* bus = registry.has_ctx<EventBus*>() ? registry.ctx<EventBus*>() : nullptr;
                    if (bus) {
                        Evt_Player_StanceChanged evt{};
                        evt.player    = id;
                        evt.oldStance = oldStance;
                        evt.newStance = PlayerStance::Standing;
                        bus->publish_deferred(evt);
                    }

                    LOG_INFO("[Sys_PlayerStance] ForceStand: " << (int)oldStance << " -> Standing");
                }
                ps.forceStandPending = false;
                return;
            }

            // 处理 forceStandPending（奔跑强制站起）
            bool cPressed = input.crouchJustPressed;
            bool vPressed = input.standJustPressed;

            if (ps.forceStandPending) {
                if (ps.stance == PlayerStance::Crouching) {
                    vPressed = true;  // 模拟 V 键按下
                }
                ps.forceStandPending = false;
            }

            PlayerStance oldStance = ps.stance;
            bool changed = false;

            // C 键：Standing → Crouching
            if (cPressed && ps.stance == PlayerStance::Standing) {
                ps.stance = PlayerStance::Crouching;
                changed = true;
            }

            // V 键：Crouching → Standing
            if (vPressed && ps.stance == PlayerStance::Crouching) {
                ps.stance = PlayerStance::Standing;
                changed = true;
            }

            if (!changed) return;

            // 确定新碰撞体半高
            float oldHalfHeight = (oldStance == PlayerStance::Standing)
                                  ? STAND_HALF_HEIGHT : CROUCH_HALF_HEIGHT;
            float newHalfHeight = (ps.stance == PlayerStance::Crouching)
                                  ? CROUCH_HALF_HEIGHT : STAND_HALF_HEIGHT;
            ps.colliderHalfHeight = newHalfHeight;

            // 1) 替换碰撞体形状
            physics->ReplaceShapeCapsule(rb.jolt_body_id, CAPSULE_RADIUS, newHalfHeight);

            // 2) 调整 Y 位置，保持脚底不动
            float oldBottom = tf.position.y - (oldHalfHeight + CAPSULE_RADIUS);
            float newCenterY = oldBottom + newHalfHeight + CAPSULE_RADIUS + 0.05f;
            physics->SetPosition(rb.jolt_body_id, tf.position.x, newCenterY, tf.position.z);
            tf.position.y = newCenterY;

            // 3) 强制激活 body
            physics->ActivateBody(rb.jolt_body_id);

            // 发布姿态切换事件
            auto* bus = registry.has_ctx<EventBus*>() ? registry.ctx<EventBus*>() : nullptr;
            if (bus) {
                Evt_Player_StanceChanged evt{};
                evt.player    = id;
                evt.oldStance = oldStance;
                evt.newStance = ps.stance;
                bus->publish_deferred(evt);
            }

            LOG_INFO("[Sys_PlayerStance] Stance: " << (int)oldStance << " -> " << (int)ps.stance);
        }
    );
}

} // namespace ECS
