#pragma once

#include "Core/ECS/EntityID.h"

namespace ECS {

/**
 * @brief 相机上下文全局资源
 *
 * 存储当前活动相机实体的 ID，供所有 System 快速访问相机信息。
 * 由 Sys_Camera 在 OnAwake 时挂载，每帧由 Sys_Camera 维护。
 *
 * 典型使用：
 * @code
 *   auto& res_cam = registry.ctx<Res_CameraContext>();
 *   auto& tf = registry.Get<C_D_Transform>(res_cam.active_camera);
 *   // tf.position 即当前相机世界坐标
 * @endcode
 *
 * @see Sys_Camera (维护者)
 * @see C_D_Camera (相机参数)
 * @see C_T_MainCamera (主相机标签)
 */
struct Res_CameraContext {
    EntityID active_camera = Entity::NULL_ENTITY; ///< 当前主相机实体 ID
};

} // namespace ECS
