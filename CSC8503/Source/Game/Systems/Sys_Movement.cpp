#include "Sys_Movement.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Physics.h"

#include <cmath>

using namespace NCL::Maths;

namespace ECS {

void Sys_Movement::OnUpdate(Registry& registry, float /*dt*/) {
    if (!registry.has_ctx<Sys_Physics*>()) return;
    auto* physics = registry.ctx<Sys_Physics*>();
    if (!physics) return;

    registry.view<C_D_Input, C_D_RigidBody, C_T_Player, C_D_PlayerState, C_D_CQCState>().each(
        [&](EntityID /*id*/, C_D_Input& input, C_D_RigidBody& rb,
            C_T_Player&, C_D_PlayerState& ps, C_D_CQCState& cqc) {
            if (!rb.body_created) return;

            // CQC 进行中：冻结水平移动，保留重力
            if (cqc.phase != CQCPhase::None) {
                Vector3 vel = physics->GetLinearVelocity(rb.jolt_body_id);
                physics->SetLinearVelocity(rb.jolt_body_id, 0.0f, vel.y, 0.0f);
                return;
            }

            Vector3 vel = physics->GetLinearVelocity(rb.jolt_body_id);

            // 从 Sys_StealthMetrics / Sys_PlayerStance 已计算好的 PlayerState 读取
            float maxSpeed = BASE_SPEED;
            float force    = BASE_FORCE;

            if (ps.isSprinting) {
                maxSpeed *= RUN_SPEED_MUL;
                force    *= RUN_SPEED_MUL;
            }

            maxSpeed *= ps.moveSpeedMul;
            force    *= ps.moveSpeedMul;

            if (input.hasInput) {
                float horizSpeed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
                if (horizSpeed < maxSpeed) {
                    physics->ActivateBody(rb.jolt_body_id);
                    physics->AddForce(rb.jolt_body_id,
                                      input.moveX * force, 0.0f, input.moveZ * force);
                }
            } else {
                // 零惯性制动：松手即停（保留 Y 轴重力速度）
                physics->SetLinearVelocity(rb.jolt_body_id, 0.0f, vel.y, 0.0f);
            }
        }
    );
}

} // namespace ECS
