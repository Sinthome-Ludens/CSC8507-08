#pragma once

#include "IScene.h"
#include "Game/Components/Res_Network.h"
#include <string>

/**
 * @brief 网络联机演示场景
 *
 * 在 Scene_PhysicsTest 的基础上增加了 Sys_Network 和 Sys_Interpolation。
 * 在 OnEnter 时根据构造传入的模式初始化 Res_Network。
 */
class Scene_NetworkGame : public IScene {
public:
    Scene_NetworkGame(ECS::PeerType mode, const std::string& ip = "127.0.0.1", uint16_t port = 32499)
        : m_Mode(mode), m_IP(ip), m_Port(port) {}

    void OnEnter(ECS::Registry&          registry,
                 ECS::SystemManager&     systems,
                 const Res_NCL_Pointers& nclPtrs) override;

    void OnExit(ECS::Registry&      registry,
                ECS::SystemManager& systems) override;

private:
    ECS::PeerType m_Mode;
    std::string   m_IP;
    uint16_t      m_Port;
};
