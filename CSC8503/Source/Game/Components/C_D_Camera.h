#pragma once

namespace ECS {

/**
 * @brief 相机参数组件（数据组件）
 *
 * 存储渲染相机的投影参数和视角信息。
 * 配合 C_D_Transform（位置）和 C_T_MainCamera（标签）使用。
 * 由 Sys_Camera 每帧读取，同步到 NCL GameWorld::mainCamera（Bridge 层）。
 *
 * 对应 Prefab：Assets/Prefabs/Prefab_Camera_Main.json
 */
struct C_D_Camera {
    float fov         = 45.0f;   ///< 垂直视野角（度），典型值 45~75
    float near_z      = 1.0f;    ///< 近裁剪面距离（米）
    float far_z       = 1000.0f; ///< 远裁剪面距离（米）
    float pitch       = 0.0f;    ///< 俯仰角（度），范围 [-89, 89]，向下为负
    float yaw         = 0.0f;    ///< 偏航角（度），绕 Y 轴旋转
    float move_speed  = 20.0f;   ///< 移动速度（单位/秒），由 WASD 键控制
    float sensitivity = 0.5f;    ///< 鼠标旋转灵敏度（度/像素）
    bool  cursor_free = false;   ///< Alt 按住时为 true：鼠标可见且不旋转相机
};

} // namespace ECS
