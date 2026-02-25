#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif

#include "Sys_Network.h"
#include "enet/enet.h"
#include "Game/Components/NetProtocol.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/Res_Time.h"
#include "Game/Events/Evt_Net_PeerConnected.h"
#include "Game/Events/Evt_Net_PeerDisconnected.h"
#include "Game/Events/Evt_Net_GameAction.h"
#include "Core/ECS/EventBus.h"
#include <iostream>

namespace ECS {

/**
 * @brief 网络系统唤醒回调
 * @details 
 * 1. 初始化 ENet 全局环境。
 * 2. 根据 Res_Network 的 mode 配置，将自己启动为 Server（监听端口）或 Client（连接服务器）。
 * 3. 实例化 enet_host 并分配本地控制权 ID。
 */
void Sys_Network::OnAwake(Registry& reg) {
    if (enet_initialize() != 0) {
        std::cerr << "[ERROR] An error occurred while initializing ENet.\n";
        return;
    }

    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.mode == PeerType::SERVER) {
        ENetAddress address;
        address.host = ENET_HOST_ANY; ///< 监听所有网卡
        address.port = 32499;

        // 创建服务端：支持最多 4 个 Client，2 个通道
        resNet.host = enet_host_create(&address, 4, 2, 0, 0);
        if (resNet.host == nullptr) {
            LOG_ERROR("An error occurred while trying to create an ENet server host.");
            return;
        }
        resNet.localClientID = 0;
        resNet.connected = true;
        LOG_INFO("Network Server started on port 32499.");
    } 
    else if (resNet.mode == PeerType::CLIENT) {
        resNet.host = enet_host_create(NULL, 1, 2, 0, 0);
        if (resNet.host == nullptr) {
            LOG_ERROR("An error occurred while trying to create an ENet client host.");
            return;
        }
        resNet.localClientID = 42; //TODO: 这里的 ClientID 应该由 Server 分配，暂时硬编码一个值用于测试

        ENetAddress address;
        enet_address_set_host(&address, "127.0.0.1");
        address.port = 32499;

        // 尝试建立连接
        resNet.peer = enet_host_connect(resNet.host, &address, 2, 0);
        if (resNet.peer == nullptr) {
            LOG_ERROR("No available peers for initiating an ENet connection.");
            return;
        }
        LOG_INFO("Network Client connecting to 127.0.0.1:32499...");
    }
}
/**
 * @details 
 * 消耗网络堆栈中的所有待处理事件：
 * 1. CONNECT/DISCONNECT: 维护连接状态并分发延迟事件。
 * 2. RECEIVE (SYNC_TRANSFORM): 核心位置同步，将收到的位置/旋转存入对应实体的环形缓冲区 C_D_InterpBuffer。
 * 3. RECEIVE (GAME_EVENT): 动作同步，如开火、跳跃，通过 EventBus 转发给逻辑系统。
 */
void Sys_Network::OnUpdate(Registry& reg, float dt) {
    auto& resNet = reg.ctx<Res_Network>();
    if (!resNet.host) return;

    ENetEvent event;
    // 轮询事件，0 表示非阻塞模式
    while (enet_host_service(resNet.host, &event, 0) > 0) {
        resNet.packetsReceived++;
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                std::cout << "[INFO] A new peer connected.\n";
                if (resNet.mode == PeerType::SERVER) {
                    uint32_t newClientID = 1;
                    event.peer->data = (void*)(uintptr_t)newClientID;
                    if (reg.has_ctx<EventBus*>()) {
                        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerConnected>({ newClientID });
                    }
                } else {
                    resNet.connected = true;
                    if (reg.has_ctx<EventBus*>()) {
                        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerConnected>({ 0u });
                    }
                }
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                resNet.bytesReceived += event.packet->dataLength;
                if (event.packet->dataLength >= sizeof(NetPacketHeader)) {
                    NetPacketHeader* header = (NetPacketHeader*)event.packet->data;
                    // 逻辑 A：处理位置同步包
                    if (header->type == SYNC_TRANSFORM) {
                        Packet_Transform* pkt = (Packet_Transform*)event.packet->data;
                        auto it = resNet.netIdMap.find(pkt->netID);
                        if (it != resNet.netIdMap.end()) {
                            EntityID target = it->second;
                            // 仅当实体拥有插值缓冲组件时才写入，用于平滑渲染
                            if (reg.Has<C_D_InterpBuffer>(target)) {
                                auto& buffer = reg.Get<C_D_InterpBuffer>(target);
                                int idx = buffer.head;
                                buffer.snapshots[idx].pos = NCL::Maths::Vector3(pkt->pos[0], pkt->pos[1], pkt->pos[2]);
                                buffer.snapshots[idx].rot = NCL::Maths::Quaternion(pkt->rot[0], pkt->rot[1], pkt->rot[2], pkt->rot[3]);
                                buffer.snapshots[idx].timestamp = (float)header->timestamp;
                                buffer.head = (buffer.head + 1) % 10;
                                if (buffer.count < 10) buffer.count++;
                            }
                        }
                    }
                    // 逻辑 B：处理远程游戏动作包（延迟分发）
                    else if (header->type == GAME_EVENT) {
                        Packet_GameAction* pkt = (Packet_GameAction*)event.packet->data;
                        if (reg.has_ctx<EventBus*>()) {
                            reg.ctx<EventBus*>()->publish_deferred<Evt_Net_GameAction>({ pkt->sourceNetID, pkt->targetNetID, pkt->actionCode, pkt->param1 });
                        }
                    }
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                LOG_INFO("Peer disconnected.");
                if (reg.has_ctx<EventBus*>()) {
                    reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerDisconnected>({ 0u });
                }
                if (resNet.mode == PeerType::CLIENT) resNet.connected = false;
                break;
            }
        }
    }

    if (!resNet.connected) return;

    m_TimeSinceLastSend += dt;
    if (m_TimeSinceLastSend < m_SendRate) return;
    m_TimeSinceLastSend = 0.0f;
    // 获取毫秒级时间戳，用于远端插值计算
    uint32_t currentTimestamp = (uint32_t)(reg.ctx<Res_Time>().totalTime * 1000.0f);

    reg.view<C_D_Transform, C_D_NetworkIdentity>().each([&](EntityID entity, C_D_Transform& tf, C_D_NetworkIdentity& net) {
        // 权限判定：只有自己控制的实体才由自己发起同步
        if (net.ownerClientID == resNet.localClientID) {
            Packet_Transform pkt;
            pkt.type = SYNC_TRANSFORM;
            pkt.timestamp = currentTimestamp;
            pkt.netID = net.netID;
            pkt.pos[0] = tf.position.x; pkt.pos[1] = tf.position.y; pkt.pos[2] = tf.position.z;
            pkt.rot[0] = tf.rotation.x; pkt.rot[1] = tf.rotation.y; pkt.rot[2] = tf.rotation.z; pkt.rot[3] = tf.rotation.w;
            // 使用不可靠传输，避免因为单包丢失导致的后续包阻塞（Head-of-line blocking）
            ENetPacket* packet = enet_packet_create(&pkt, sizeof(Packet_Transform), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
            if (resNet.mode == PeerType::SERVER) {
                enet_host_broadcast(resNet.host, 0, packet);
            } else {
                if (resNet.peer) enet_peer_send(resNet.peer, 0, packet);
            }
            resNet.packetsSent++;
            resNet.bytesSent += sizeof(Packet_Transform);
        }
    });
}
/**
 * @details 
 * 1. 检查发包计时器（m_SendRate）。
 * 2. 遍历所有带 NetworkIdentity 的实体，筛选出“本地掌控”的实体。
 * 3. 将本地最新的 Transform 数据打包，根据 Peer 身份进行广播或定向发送。
 * 4. 位置同步使用 UNRELIABLE (不可靠传输)，允许丢包以换取高实时性。
 */
void Sys_Network::OnFixedUpdate(Registry& reg, float dt) {} //TODO: 可以在这里实现基于固定频率的游戏事件同步，例如玩家输入、开火等动作，确保服务器和客户端的游戏状态一致,目前ECS框架的 FixedUpdate 还没有实现，暂时放在 Update 中处理

/**
 * @brief 系统清理
 * @details 关闭 ENet Host 并释放所有底层网络资源，重置初始化环境。
 */
void Sys_Network::OnDestroy(Registry& reg) {
    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.host) {
        enet_host_destroy(resNet.host);
        resNet.host = nullptr;
    }
    enet_deinitialize();
    LOG_INFO("Network System shut down. Sent: " << resNet.packetsSent << ", Received: " << resNet.packetsReceived);
}
} // namespace ECS