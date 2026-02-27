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
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/Res_Time.h"
#include "Game/Events/Evt_Net_PeerConnected.h"
#include "Game/Events/Evt_Net_PeerDisconnected.h"
#include "Game/Events/Evt_Net_GameAction.h"
#include "Core/ECS/EventBus.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerInput.h"
#include "Keyboard.h"
#include <iostream>

namespace ECS {

/**
 * @brief 注册网络数据包类型的处理函数映射
 */
void Sys_Network::RegisterHandlers() {
    m_PacketHandlers[SYS_WELCOME]    = &Sys_Network::HandleWelcomePacket;
    m_PacketHandlers[SYNC_TRANSFORM] = &Sys_Network::HandleSyncTransform;
    m_PacketHandlers[CLIENT_INPUT]   = &Sys_Network::HandleClientInput;
    m_PacketHandlers[GAME_EVENT]     = &Sys_Network::HandleGameAction;
}

/**
 * @brief 系统初始化阶段，配置事件并初始化 ENet 网络环境
 * @param reg ECS 注册表
 */
void Sys_Network::OnAwake(Registry& reg) {
    RegisterHandlers();
    InitializeEvents(reg);

    if (enet_initialize() != 0) {
        LOG_ERROR("An error occurred while initializing ENet.");
        return;
    }

    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.mode == PeerType::SERVER) {
        InitializeServer(resNet);
    } 
    else if (resNet.mode == PeerType::CLIENT) {
        InitializeClient(resNet);
    }
}

/**
 * @brief 初始化事件总线并注册网络相关事件的监听器
 * @param reg ECS 注册表
 */
void Sys_Network::InitializeEvents(Registry& reg) {
    // 缓存 Registry 指针供 EventBus 回调使用
    m_Registry = &reg;
    
    // 确保 EventBus 存在并注册事件监听
    if (!reg.has_ctx<EventBus*>()) {
        m_EventBus = std::make_unique<EventBus>();
        reg.ctx_emplace<EventBus*>(m_EventBus.get());
    }
    
    // 捕获 this 指针并绑定成员函数
    m_ActionSubID = reg.ctx<EventBus*>()->subscribe<Evt_Net_GameAction>(
        [this](const Evt_Net_GameAction& evt) { this->OnLocalGameAction(evt); }
    );
}

/**
 * @brief 将当前节点初始化为服务器，监听默认端口等待连接
 * @param resNet 网络资源对象，用于存储服务器主机与状态
 */
void Sys_Network::InitializeServer(Res_Network& resNet) {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 32499;

    resNet.host = enet_host_create(&address, 4, 2, 0, 0);
    if (resNet.host == nullptr) {
        LOG_ERROR("An error occurred while trying to create an ENet server host.");
        return;
    }
    resNet.localClientID = 0;
    resNet.connected = true;
    m_NextClientID = 1; 
    LOG_INFO("Network Server started on port 32499.");
}

/**
 * @brief 将当前节点初始化为客户端，并向指定的服务器发起连接
 * @param resNet 网络资源对象，用于存储客户端主机与连接状态
 */
void Sys_Network::InitializeClient(Res_Network& resNet) {
    resNet.host = enet_host_create(NULL, 1, 2, 0, 0);
    if (resNet.host == nullptr) {
        LOG_ERROR("An error occurred while trying to create an ENet client host.");
        return;
    }
    // 客户端实际 ID 应在 SYS_WELCOME 包中由 Server 分配。
    // 初始化为最大值表示尚未分配
    resNet.localClientID = UINT32_MAX;

    ENetAddress address;
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 32499;

    resNet.peer = enet_host_connect(resNet.host, &address, 2, 0);
    if (resNet.peer == nullptr) {
        LOG_ERROR("No available peers for initiating an ENet connection.");
        return;
    }
    m_LastInputMask = 0;
    LOG_INFO("Network Client connecting to 127.0.0.1:32499...");
}

/**
 * @brief 每帧更新函数，处理网络事件、收集本地输入并进行状态广播
 * @param reg ECS 注册表
 * @param dt 帧耗时
 */
void Sys_Network::OnUpdate(Registry& reg, float dt) {
    auto& resNet = reg.ctx<Res_Network>();
    if (!resNet.host) return;

    // 1. 处理所有传入的网络事件
    ProcessNetworkEvents(reg, resNet);
    if (!resNet.connected) return;

    // 2. 处理本地输入（客户端发包，服务端直接驱动物理）
    HandleLocalInput(reg, resNet);

    // 3. 服务端定时广播同步状态
    m_TimeSinceLastSend += dt;
    if (m_TimeSinceLastSend >= m_SendRate && resNet.mode == PeerType::SERVER) {
        m_TimeSinceLastSend -= m_SendRate;
        if (m_TimeSinceLastSend > m_SendRate) m_TimeSinceLastSend = 0.0f; // prevent spiral
        BroadcastWorldState(reg, resNet);
    }
}
/**
 * @brief 轮询并处理底层的 ENet 网络事件（连接、接收数据、断开）
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::ProcessNetworkEvents(Registry& reg, Res_Network& resNet) {
    ENetEvent event;
    while (enet_host_service(resNet.host, &event, 0) > 0) {
        resNet.packetsReceived++;
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                std::cout << "[INFO] A new peer connected.\n";
                if (resNet.mode == PeerType::SERVER) {
                    uint32_t newClientID = m_NextClientID++;
                    event.peer->data = (void*)(uintptr_t)newClientID;
                    if (reg.has_ctx<EventBus*>()) {
                        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerConnected>({ newClientID });
                    }
                    
                    // 发送 Welcome 数据包分配 ID 给客户端
                    Net_Packet_Welcome welcomePkt;
                    welcomePkt.type = SYS_WELCOME;
                    welcomePkt.clientID = newClientID;
                    ENetPacket* packet = enet_packet_create(&welcomePkt, sizeof(Net_Packet_Welcome), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);
                } else {
                    resNet.connected = true;
                    // Client 侧不在此处抛出事件，在收到 SYS_WELCOME 知道自己 ID 时再抛出
                }
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                resNet.bytesReceived += event.packet->dataLength;
                HandleReceivePacket(reg, resNet, event);
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
}

/**
 * @brief 接收数据包的分发器，根据包头类型调用对应的处理函数
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event 包含接收数据包的 ENet 事件
 */
void Sys_Network::HandleReceivePacket(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (event.packet->dataLength < sizeof(Net_PacketHeader)) return;

    Net_PacketHeader* header = (Net_PacketHeader*)event.packet->data;
    auto it = m_PacketHandlers.find(header->type);
    
    if (it != m_PacketHandlers.end()) {
        auto handlerFunc = it->second;
        (this->*handlerFunc)(reg, resNet, event);
    } else {
        LOG_WARN("Received unknown packet type: " << (int)header->type);
    }
}

// --- 数据包处理回调函数实现 ---

/**
 * @brief 处理 SYS_WELCOME 数据包（仅客户端），接收服务器分配的 ClientID
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleWelcomePacket(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::CLIENT) return;
    
    auto* pkt = GetPacketData<Net_Packet_Welcome>(event);
    if (!pkt) return;

    resNet.localClientID = pkt->clientID;
    LOG_INFO("Received SYS_WELCOME. Assigned Client ID: " << resNet.localClientID);
    
    // 此时已经收到真实的 ClientID，可以在这里抛出事件通知其他系统进行玩家实体的创建等逻辑
    if (reg.has_ctx<EventBus*>()) {
        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerConnected>({ pkt->clientID });
    }
}

/**
 * @brief 处理 SYNC_TRANSFORM 数据包（仅客户端），将服务器同步的位置数据存入插值缓冲区
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleSyncTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::CLIENT) return;

    auto* pkt = GetPacketData<Net_Packet_Transform>(event);
    if (!pkt) return;

    auto it = resNet.netIdMap.find(pkt->netID);
    if (it != resNet.netIdMap.end()) {
        EntityID target = it->second;
        if (reg.Has<C_D_InterpBuffer>(target)) {
            auto& buffer = reg.Get<C_D_InterpBuffer>(target);
            buffer.AddSnapshot(
                NCL::Maths::Vector3(pkt->pos[0], pkt->pos[1], pkt->pos[2]),
                NCL::Maths::Quaternion(pkt->rot[0], pkt->rot[1], pkt->rot[2], pkt->rot[3]),
                (float)pkt->timestamp
            );
        }
    }
}

/**
 * @brief 处理 CLIENT_INPUT 数据包（仅服务端），接收并更新对应客户端的按键输入状态
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleClientInput(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::SERVER) return;

    auto* pkt = GetPacketData<Net_Packet_ClientInput>(event);
    if (!pkt) return;
    UpdatePlayerInput(reg, GetClientID(event), pkt->buttonMask);
}

/**
 * @brief 处理 GAME_EVENT 数据包，将网络接收到的游戏动作作为本地事件抛出供其他系统消费
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleGameAction(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    auto* pkt = GetPacketData<Net_Packet_GameAction>(event);
    if (!pkt) return;
    if (reg.has_ctx<EventBus*>()) {
        Evt_Net_GameAction localEvt;
        localEvt.sourceNetID = pkt->sourceNetID;
        localEvt.targetNetID = pkt->targetNetID;
        localEvt.actionCode = pkt->actionCode;
        localEvt.param1 = pkt->param1;
        localEvt.isLocalOrigin = false; // <--- 关键：标记为来自网络，防止本地 Sys_Network 再次监听并发出死循环
        
        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_GameAction>(localEvt);
    }
}

/**
 * @brief 收集本地玩家输入，客户端会将其打包发送给服务端，服务端则直接应用于本地控制的实体
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::HandleLocalInput(Registry& reg, Res_Network& resNet) {
    if (!reg.has_ctx<Res_Input>()) return;
    auto& input = reg.ctx<Res_Input>();

    uint32_t currentMask = 0;
    if (input.keyStates[NCL::KeyCodes::UP])    currentMask |= PlayerInputFlags::Up;
    if (input.keyStates[NCL::KeyCodes::DOWN])  currentMask |= PlayerInputFlags::Down;
    if (input.keyStates[NCL::KeyCodes::LEFT])  currentMask |= PlayerInputFlags::Left;
    if (input.keyStates[NCL::KeyCodes::RIGHT]) currentMask |= PlayerInputFlags::Right;
    
    // --- 1. Client：收集输入并发送给 Server ---
    if (resNet.mode == PeerType::CLIENT && resNet.peer != nullptr) {
        Net_Packet_ClientInput pkt;
        pkt.type = CLIENT_INPUT;
        pkt.timestamp = (uint32_t)(reg.ctx<Res_Time>().totalTime * 1000.0f);
        pkt.buttonMask = currentMask;
        
                bool isMoving = (currentMask != 0);
                bool stateChanged = (currentMask != m_LastInputMask);
        
                static float inputTimer = 0.0f;
                inputTimer += reg.ctx<Res_Time>().deltaTime;
        
                if (stateChanged) {
                    // 状态改变时（无论是按下新键还是松开），发送一次可靠包
                    SendPacket(resNet, pkt, false, true);
                    inputTimer = 0.0f; // 重置计时器
                } else if (isMoving && inputTimer >= m_SendRate) {
                    // 持续按键时，按照设定频率发送不可靠包
                    SendPacket(resNet, pkt, false, false);
                    inputTimer -= m_SendRate;
                    if (inputTimer > m_SendRate) inputTimer = 0.0f; // 防止螺旋
                }
                m_LastInputMask = currentMask;    }    

    // --- 2. Server：处理主机本地玩家的输入 ---
    if (resNet.mode == PeerType::SERVER) {
        UpdatePlayerInput(reg, resNet.localClientID, currentMask);
    }
}

/**
 * @brief 将输入位掩码应用到特定客户端所拥有的实体上
 * @param reg ECS 注册表
 * @param clientID 对应的客户端唯一标识 ID
 * @param buttonMask 玩家输入的按键位掩码
 */
void Sys_Network::UpdatePlayerInput(Registry& reg, uint32_t clientID, uint32_t buttonMask) {
    reg.view<C_D_NetworkIdentity>().each(
        [&](EntityID id, C_D_NetworkIdentity& net) {
            if (net.ownerClientID == clientID) {
                if (!reg.Has<C_D_PlayerInput>(id)) {
                    reg.Emplace<C_D_PlayerInput>(id);
                }
                auto& input = reg.Get<C_D_PlayerInput>(id);
                input.buttonMask = buttonMask;
            }
        }
    );
}

/**
 * @brief （仅服务端）将所有网络实体的最新变换信息（Transform）打包广播给所有连接的客户端
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::BroadcastWorldState(Registry& reg, Res_Network& resNet) {
    uint32_t currentTimestamp = (uint32_t)(reg.ctx<Res_Time>().totalTime * 1000.0f);

    reg.view<C_D_Transform, C_D_NetworkIdentity>().each([&](EntityID entity, C_D_Transform& tf, C_D_NetworkIdentity& net) {
        Net_Packet_Transform pkt;
        pkt.type = SYNC_TRANSFORM;
        pkt.timestamp = currentTimestamp;
        pkt.netID = net.netID;
        pkt.pos[0] = tf.position.x; pkt.pos[1] = tf.position.y; pkt.pos[2] = tf.position.z;
        pkt.rot[0] = tf.rotation.x; pkt.rot[1] = tf.rotation.y; pkt.rot[2] = tf.rotation.z; pkt.rot[3] = tf.rotation.w;
        
        SendPacket(resNet, pkt, true);
    });
}

/**
 * @brief 固定物理帧更新，目前暂未使用
 * @param reg ECS 注册表
 * @param dt 固定时间步长
 */
void Sys_Network::OnFixedUpdate(Registry& reg, float dt) {}

/**
 * @brief 系统销毁阶段，清理 ENet 资源与事件订阅，避免内存泄漏
 * @param reg ECS 注册表
 */
void Sys_Network::OnDestroy(Registry& reg) {
    // 取消订阅，防止析构后依然收到回调
    if (reg.has_ctx<EventBus*>()) {
        EventBus* bus = reg.ctx<EventBus*>();
        if (bus) {
            bus->unsubscribe<Evt_Net_GameAction>(m_ActionSubID);
        }
        // 如果此系统拥有并注册了 EventBus，将其从上下文中移除（置空）
        if (m_EventBus && bus == m_EventBus.get()) {
            reg.ctx_erase<EventBus*>();
        }
    }
    m_EventBus.reset();
    m_Registry = nullptr;

    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.host) {
        enet_host_destroy(resNet.host);
        resNet.host = nullptr;
    }
    enet_deinitialize();
    LOG_INFO("Network System shut down. Sent: " << resNet.packetsSent << ", Received: " << resNet.packetsReceived);
}

/**
 * @brief 监听本地产生的游戏动作事件，并将其打包发送到网络中
 * @param evt 游戏动作事件对象
 */
void Sys_Network::OnLocalGameAction(const Evt_Net_GameAction& evt) {
    // 1. 防止死循环回音：如果这个事件是从网络收到的，就不再发回网络
    if (!evt.isLocalOrigin) return;

    // 2. 检查资源有效性
    if (!m_Registry || !m_Registry->has_ctx<Res_Network>() || !m_Registry->has_ctx<Res_Time>()) return;
    
    auto& resNet = m_Registry->ctx<Res_Network>();
    if (!resNet.connected || !resNet.host) return;

    // 3. 构建并发送网络包
    Net_Packet_GameAction pkt;
    pkt.type = GAME_EVENT;
    pkt.timestamp = (uint32_t)(m_Registry->ctx<Res_Time>().totalTime * 1000.0f);
    pkt.sourceNetID = evt.sourceNetID;
    pkt.targetNetID = evt.targetNetID;
    pkt.actionCode  = evt.actionCode;
    pkt.param1      = evt.param1;

    // Server 广播给所有人，Client 发送给 Server
    bool isServer = (resNet.mode == PeerType::SERVER);
    SendPacket(resNet, pkt, isServer, true);
}

} // namespace ECS