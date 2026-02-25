#pragma once

#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_Network.h"
#include "Game/Utils/Log.h"

namespace ECS {

/**
 * @brief 网络系统：负责 ENet 的初始化、网络数据的收发与解析。
 *
 * @details
 * - Server：监听特定端口（默认 32499），接收新客户端连接并验证。
 * - Client：连接至 Server，响应 Server 的状态更新。
 * - 收集实体 Transform 数据，根据网络协议进行广播/发送。
 * 
 * 依赖的系统/组件：
 * - 写入：C_D_InterpBuffer (用于远程实体的位姿更新)
 * - 读取：C_D_Transform, C_D_NetworkIdentity, Res_Network
 */
class Sys_Network : public ISystem {
public:
    /**
     * @brief 初始化网络环境
     * @param reg ECS 注册表
     */
    void OnAwake(Registry& reg) override;

    /**
     * @brief 轮询网络事件并处理接收数据
     * @param reg ECS 注册表
     * @param dt 帧耗时
     */
    void OnUpdate(Registry& reg, float dt) override;

    /**
     * @brief 处理网络数据的发送逻辑，通常基于固定频率
     * @param reg ECS 注册表
     * @param dt 固定时间步长
     */
    void OnFixedUpdate(Registry& reg, float dt) override;

    /**
     * @brief 清理网络资源
     * @param reg ECS 注册表
     */
    void OnDestroy(Registry& reg) override;

private:
    float m_TimeSinceLastSend = 0.0f;
    const float m_SendRate = 1.0f / 20.0f; // 默认发送频率为 20Hz
};

} // namespace ECS
