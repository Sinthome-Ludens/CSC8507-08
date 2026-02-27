#pragma once

#include "Core/ECS/SystemManager.h"
#include "Core/ECS/EventBus.h"
#include "Game/Components/Res_Network.h"
#include "Game/Events/Evt_Net_GameAction.h"
#include "Game/Utils/Log.h"
#include "enet/enet.h"
#include "Game/Network/Net_Protocol.h"
#include <unordered_map>
#include <functional>

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

    // ── 分发器 (Dispatcher) 模式 ──
    using PacketHandler = void (Sys_Network::*)(Registry&, Res_Network&, const ENetEvent&);
    std::unordered_map<Net_PacketType, PacketHandler> m_PacketHandlers;

    void RegisterHandlers();

    // ── 初始化阶段函数 ──
    void InitializeEvents(Registry& reg);
    void InitializeServer(Res_Network& resNet);
    void InitializeClient(Res_Network& resNet);

    // ── 内部阶段函数 ──
    void ProcessNetworkEvents(Registry& reg, Res_Network& resNet);
    void HandleReceivePacket(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    void HandleLocalInput(Registry& reg, Res_Network& resNet);
    void BroadcastWorldState(Registry& reg, Res_Network& resNet);

    // ── 数据包处理回调函数 ──
    void HandleWelcomePacket(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    void HandleSyncTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    void HandleClientInput(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    void HandleGameAction(Registry& reg, Res_Network& resNet, const ENetEvent& event);

    // ── 数据包解包与物理驱动辅助方法 ──
    template<typename T>
    T* GetPacketData(const ENetEvent& event) {
        if (event.packet->dataLength < sizeof(T)) return nullptr;
        return reinterpret_cast<T*>(event.packet->data);
    }

    static uint32_t GetClientID(const ENetEvent& event) {
        return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
    }

    /**
     * @brief 统一物理驱动逻辑：根据输入方向给指定 Client 拥有的实体施加速度。
     */
    void UpdatePlayerInput(Registry& reg, uint32_t clientID, bool up, bool down, bool left, bool right);

    /**
     * @brief 统一发送辅助函数：封装数据包创建、发送及统计逻辑。
     * @tparam T 数据包结构体类型
     * @param resNet 网络资源引用
     * @param packet 要发送的数据包对象
     * @param broadcast 是否广播（true=广播，false=发送给 resNet.peer）
     */
    template<typename T>
    void SendPacket(Res_Network& resNet, T& packet, bool broadcast = false) {
        ENetPacket* p = enet_packet_create(&packet, sizeof(T), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
        if (broadcast) {
            enet_host_broadcast(resNet.host, 0, p);
        } else if (resNet.peer) {
            enet_peer_send(resNet.peer, 0, p);
        } else {
            enet_packet_destroy(p);
            return;
        }
        resNet.packetsSent++;
        resNet.bytesSent += sizeof(T);
    }

    // ── 事件监听与发送 ──
    Registry* m_Registry = nullptr; ///< 缓存的注册表指针，用于在事件回调中获取网络资源
    SubscriptionID m_ActionSubID = 0; ///< 保存事件订阅的 ID
    void OnLocalGameAction(const Evt_Net_GameAction& evt);
};

} // namespace ECS
