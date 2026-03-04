#pragma once

#include "IScene.h"

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
