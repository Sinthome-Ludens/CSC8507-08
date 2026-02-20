#pragma once

#include "Core/ECS/BaseSystem.h"

namespace NCL::CSC8503 { class GameWorld; }

namespace ECS {

/**
 * @brief 相机系统
 *
 * 职责：
 *   - OnAwake：通过 PrefabFactory 创建相机实体，注册 Res_CameraContext，
 *              初始化场景光照（Bridge），完成首帧同步。
 *   - OnUpdate：读取 WASD/鼠标输入，更新相机实体 C_D_Transform 和 C_D_Camera，
 *               再通过 Bridge 同步到 NCL GameWorld::mainCamera。
 *   - OnDestroy：释放 Bridge 引用。
 *
 * 执行优先级：50（早于 Sys_Physics=100、Sys_Render=200）
 *
 * 键位：
 *   W/S/A/D  — 前后左右移动
 *   Q/E      — 下降/上升
 *   鼠标移动  — 旋转视角（需 Window::LockMouseToWindow(true)）
 *   Alt（按住）— 显示鼠标光标，鼠标不再旋转相机；松开恢复锁定
 */
class Sys_Camera : public ISystem {
public:
    Sys_Camera() = default;

    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    NCL::CSC8503::GameWorld* m_GameWorld = nullptr; ///< Bridge：NCL 世界引用（非游戏状态）
};

} // namespace ECS
