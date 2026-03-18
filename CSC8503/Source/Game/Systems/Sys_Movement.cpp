/**
 * @file Sys_Movement.cpp
 * @brief 玩家移动系统实现。
 *
 * @details
 * 根据输入与玩家状态驱动 EntityID 语义下的物理移动接口。
 */
#include "Sys_Movement.h"
#include "Game/Utils/PauseGuard.h"

#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Physics.h"

#include <cmath>

using namespace NCL::Maths;

namespace ECS {

/**
 * @brief 处理玩家水平移动与制动。
 * @details 从输入和玩家状态计算本帧推力或刹车速度，并通过 Sys_Physics 的 EntityID 接口作用到玩家刚体；CQC 过程中仅冻结水平速度并保留重力分量。
 * @param registry 当前场景注册表
 * @param dt 本帧时间步长（当前实现未直接使用）
 */
void Sys_Movement::OnUpdate(Registry& registry, float /*dt*/) {
    PAUSE_GUARD(registry);
    if (!registry.has_ctx<Sys_Physics*>()) return;
    auto* physics = registry.ctx<Sys_Physics*>();
    if (!physics) return;

    registry.view<C_D_Input, C_D_RigidBody, C_T_Player, C_D_PlayerState, C_D_CQCState>().each(
        [&](EntityID id, C_D_Input& input, C_D_RigidBody& rb,
            C_T_Player&, C_D_PlayerState& ps, C_D_CQCState& cqc) {
            if (!rb.body_created) return;

            // CQC 进行中：冻结水平移动，保留重力
            if (cqc.phase != CQCPhase::None) {
                Vector3 vel = physics->GetLinearVelocity(id);
                physics->SetLinearVelocity(id, 0.0f, vel.y, 0.0f);
                return;
            }

            Vector3 vel = physics->GetLinearVelocity(id);

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
                    physics->ActivateBody(id);
                    physics->AddForce(id,
                                      input.moveX * force, 0.0f, input.moveZ * force);
                }
            } else {
                // 零惯性制动：松手即停（保留 Y 轴重力速度）
                physics->SetLinearVelocity(id, 0.0f, vel.y, 0.0f);
            }
        }
    );
}

} // namespace ECS
