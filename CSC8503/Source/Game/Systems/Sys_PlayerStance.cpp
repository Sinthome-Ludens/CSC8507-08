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

namespace {
    constexpr float SKIN_OFFSET = 0.05f; // 防止碰撞体嵌入地面的皮肤偏移
}

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
                    physics->ReplaceShapeCapsule(rb.jolt_body_id, STAND_HALF_HEIGHT, CAPSULE_RADIUS);

                    float oldBottom = (tf.position.y - SKIN_OFFSET) - (oldHalfHeight + CAPSULE_RADIUS);
                    float newCenterY = oldBottom + STAND_HALF_HEIGHT + CAPSULE_RADIUS + SKIN_OFFSET;
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

            // 根据输入计算本帧目标姿态，只允许一次状态切换
            PlayerStance newStance = ps.stance;

            // C 键：Standing → Crouching
            if (cPressed && ps.stance == PlayerStance::Standing) {
                newStance = PlayerStance::Crouching;
            }
            // V 键：Crouching → Standing（与 C 键互斥，同一帧只生效一个）
            else if (vPressed && ps.stance == PlayerStance::Crouching) {
                newStance = PlayerStance::Standing;
            }

            // 如果最终姿态与原姿态相同，则不做任何处理
            if (newStance == oldStance) return;

            ps.stance = newStance;

            // 确定新碰撞体半高
            float oldHalfHeight = (oldStance == PlayerStance::Standing)
                                  ? STAND_HALF_HEIGHT : CROUCH_HALF_HEIGHT;
            float newHalfHeight = (ps.stance == PlayerStance::Crouching)
                                  ? CROUCH_HALF_HEIGHT : STAND_HALF_HEIGHT;
            ps.colliderHalfHeight = newHalfHeight;

            // 1) 替换碰撞体形状
            physics->ReplaceShapeCapsule(rb.jolt_body_id, newHalfHeight, CAPSULE_RADIUS);

            // 2) 调整 Y 位置，保持脚底不动
            float oldBottom = (tf.position.y - SKIN_OFFSET) - (oldHalfHeight + CAPSULE_RADIUS);
            float newCenterY = oldBottom + newHalfHeight + CAPSULE_RADIUS + SKIN_OFFSET;
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
