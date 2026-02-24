#pragma once

#include "IScene.h"

/**
 * @brief 主菜单场景
 *
 * 仅注册UI相关系统：
 *   - Sys_ImGui (300) — Debug窗口 [仅 USE_IMGUI + _DEBUG]
 *   - Sys_UI    (500) — 游戏UI（Splash/主菜单/设置）
 *
 * 不需要物理/渲染/相机系统。
 */
class Scene_MainMenu : public IScene {
public:
    Scene_MainMenu()  = default;
    ~Scene_MainMenu() = default;

    void OnEnter(ECS::Registry&          registry,
                 ECS::SystemManager&     systems,
                 const Res_NCL_Pointers& nclPtrs) override;

    void OnExit(ECS::Registry&      registry,
                ECS::SystemManager& systems) override;
};
