#include "Sys_PlayerCamera.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Camera.h"
#include <algorithm>

using namespace NCL::Maths;

namespace ECS {

void Sys_PlayerCamera::OnUpdate(Registry& registry, float dt) {
    // Debug 模式检查：纯自由飞行时跳过，Sync 模式时跟随但不锁定旋转
    bool syncMode = false;
    if (registry.has_ctx<Sys_Camera*>()) {
        auto* cam = registry.ctx<Sys_Camera*>();
        if (cam && cam->IsDebugMode()) {
            if (!cam->IsSyncToPlayer()) return;  // 纯自由飞行：跳过
            syncMode = true;  // Sync 模式：跟随位置，不锁定旋转
        }
    }

    // 1. 找到玩家位置
    Vector3 playerPos{0, 0, 0};
    bool foundPlayer = false;

    registry.view<C_D_Transform, C_T_Player>().each(
        [&](EntityID /*id*/, C_D_Transform& tf, C_T_Player&) {
            playerPos = tf.position;
            foundPlayer = true;
        }
    );

    if (!foundPlayer) return;

    // 2. 更新主相机
    registry.view<C_T_MainCamera, C_D_Camera, C_D_Transform>().each(
        [&](EntityID /*id*/, C_T_MainCamera&, C_D_Camera& cam, C_D_Transform& camTf) {
            // 目标位置 = 玩家位置 + 偏移
            Vector3 targetPos = playerPos + CAMERA_OFFSET;

            // Lerp 平滑跟随
            float t = std::min(1.0f, SMOOTH_SPEED * dt);
            camTf.position = camTf.position + (targetPos - camTf.position) * t;

            // Sync 模式：不设置 pitch/yaw，由 Sys_Camera (155) 通过鼠标控制
            if (!syncMode) {
                cam.pitch = FIXED_PITCH;
                cam.yaw   = FIXED_YAW;
            }
        }
    );
}

} // namespace ECS
