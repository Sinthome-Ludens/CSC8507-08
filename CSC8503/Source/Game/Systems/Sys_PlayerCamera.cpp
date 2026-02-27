#include "Sys_PlayerCamera.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Utils/Log.h"

#include <algorithm>

using namespace NCL::Maths;

namespace ECS {

void Sys_PlayerCamera::OnUpdate(Registry& registry, float dt) {
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

            // 固定视角
            cam.pitch = FIXED_PITCH;
            cam.yaw   = FIXED_YAW;
        }
    );
}

} // namespace ECS
