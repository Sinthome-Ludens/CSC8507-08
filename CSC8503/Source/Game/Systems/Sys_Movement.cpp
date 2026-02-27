#include "Sys_Movement.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Physics.h"

#include <cmath>

using namespace NCL::Maths;

namespace ECS {

void Sys_Movement::OnUpdate(Registry& registry, float dt) {
    if (!registry.has_ctx<Sys_Physics*>()) return;
    auto* physics = registry.ctx<Sys_Physics*>();
    if (!physics) return;

    registry.view<C_D_Input, C_D_RigidBody, C_T_Player, C_D_PlayerState>().each(
        [&](EntityID /*id*/, C_D_Input& input, C_D_RigidBody& rb,
            C_T_Player&, C_D_PlayerState& ps) {
            if (!rb.body_created) return;

            Vector3 vel = physics->GetLinearVelocity(rb.jolt_body_id);

            // 从 Sys_Gameplay 已计算好的 PlayerState 读取
            float stanceMul = (ps.stance == PlayerStance::Crouching) ? 0.5f : 1.0f;

            float maxSpeed = BASE_SPEED;
            float force    = BASE_FORCE;

            // ps.isSprinting 由上游系统（Sys_StealthMetrics）写入；
            // 若上游尚未注册，回退到直接读取 input.shiftDown
            if (ps.isSprinting || input.shiftDown) {
                maxSpeed *= RUN_SPEED_MUL;
                force    *= RUN_SPEED_MUL;
            }

            maxSpeed *= ps.moveSpeedMul;
            force    *= stanceMul;  // force 只受姿态影响；其它减速（如伪装）通过 ps.moveSpeedMul 仅体现在 maxSpeed 上

            if (input.hasInput) {
                float horizSpeed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
                if (horizSpeed < maxSpeed) {
                    physics->ActivateBody(rb.jolt_body_id);
                    float ix = input.moveX * force * dt;
                    float iz = input.moveZ * force * dt;
                    physics->ApplyImpulse(rb.jolt_body_id, ix, 0.0f, iz);
                }
            } else {
                // 零惯性制动：松手即停（保留 Y 轴重力速度）
                physics->SetLinearVelocity(rb.jolt_body_id, 0.0f, vel.y, 0.0f);
            }
        }
    );
}

} // namespace ECS
