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
#include "Keyboard.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <iostream>

namespace ECS {

void Sys_Network::RegisterHandlers() {
    m_PacketHandlers[SYNC_TRANSFORM] = &Sys_Network::HandleSyncTransform;
    m_PacketHandlers[CLIENT_INPUT]   = &Sys_Network::HandleClientInput;
    m_PacketHandlers[GAME_EVENT]     = &Sys_Network::HandleGameAction;
}

void Sys_Network::OnAwake(Registry& reg) {
    RegisterHandlers();
    
    // 缓存 Registry 指针供 EventBus 回调使用
    m_Registry = &reg;
    
    // 确保 EventBus 存在并注册事件监听
    if (!reg.has_ctx<EventBus*>()) {
        reg.ctx_emplace<EventBus*>(new EventBus());
    }
    
    // 捕获 this 指针并绑定成员函数
    m_ActionSubID = reg.ctx<EventBus*>()->subscribe<Evt_Net_GameAction>(
        [this](const Evt_Net_GameAction& evt) { this->OnLocalGameAction(evt); }
    );

    if (enet_initialize() != 0) {
        std::cerr << "[ERROR] An error occurred while initializing ENet.\n";
        return;
    }

    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.mode == PeerType::SERVER) {
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
        LOG_INFO("Network Server started on port 32499.");
    } 
    else if (resNet.mode == PeerType::CLIENT) {
        resNet.host = enet_host_create(NULL, 1, 2, 0, 0);
        if (resNet.host == nullptr) {
            LOG_ERROR("An error occurred while trying to create an ENet client host.");
            return;
        }
        // 注意：客户端实际 ID 应在 SYS_WELCOME 包中由 Server 分配。
        // 由于测试场景中写死为 1，这里也暂时使用 1 保证匹配 Server 分配的新 ID。
        resNet.localClientID = 1;

        ENetAddress address;
        enet_address_set_host(&address, "127.0.0.1");
        address.port = 32499;

        resNet.peer = enet_host_connect(resNet.host, &address, 2, 0);
        if (resNet.peer == nullptr) {
            LOG_ERROR("No available peers for initiating an ENet connection.");
            return;
        }
        LOG_INFO("Network Client connecting to 127.0.0.1:32499...");
    }
}

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
        m_TimeSinceLastSend = 0.0f;
        BroadcastWorldState(reg, resNet);
    }
}

void Sys_Network::ProcessNetworkEvents(Registry& reg, Res_Network& resNet) {
    ENetEvent event;
    while (enet_host_service(resNet.host, &event, 0) > 0) {
        resNet.packetsReceived++;
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                std::cout << "[INFO] A new peer connected.\n";
                if (resNet.mode == PeerType::SERVER) {
                    uint32_t newClientID = 1; // 临时分配的硬编码ID
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

void Sys_Network::HandleClientInput(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::SERVER) return;

    auto* pkt = GetPacketData<Net_Packet_ClientInput>(event);
    if (!pkt) return;
    ApplyMovement(reg, GetClientID(event), pkt->up, pkt->down, pkt->left, pkt->right);
}

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

void Sys_Network::HandleLocalInput(Registry& reg, Res_Network& resNet) {
    if (!reg.has_ctx<Res_Input>()) return;
    auto& input = reg.ctx<Res_Input>();
    
    // --- 1. Client：收集输入并发送给 Server ---
    if (resNet.mode == PeerType::CLIENT && resNet.peer != nullptr) {
        Net_Packet_ClientInput pkt;
        pkt.type = CLIENT_INPUT;
        pkt.timestamp = (uint32_t)(reg.ctx<Res_Time>().totalTime * 1000.0f);
        pkt.up = input.keyStates[NCL::KeyCodes::UP];
        pkt.down = input.keyStates[NCL::KeyCodes::DOWN];
        pkt.left = input.keyStates[NCL::KeyCodes::LEFT];
        pkt.right = input.keyStates[NCL::KeyCodes::RIGHT];
        
        if (pkt.up || pkt.down || pkt.left || pkt.right) {
            SendPacket(resNet, pkt, false);
        }
    }
    
    // --- 2. Server：处理主机本地玩家的输入 ---
    if (resNet.mode == PeerType::SERVER) {
        ApplyMovement(reg, resNet.localClientID, input.keyStates[NCL::KeyCodes::UP], 
                      input.keyStates[NCL::KeyCodes::DOWN], 
                      input.keyStates[NCL::KeyCodes::LEFT], 
                      input.keyStates[NCL::KeyCodes::RIGHT]);
    }
}

void Sys_Network::ApplyMovement(Registry& reg, uint32_t clientID, bool up, bool down, bool left, bool right) {
    if (!reg.has_ctx<JPH::PhysicsSystem*>()) return;
    auto& bi = reg.ctx<JPH::PhysicsSystem*>()->GetBodyInterface();
    const float speed = 10.0f;

    reg.view<C_D_NetworkIdentity, C_D_RigidBody>().each(
        [&](EntityID /*id*/, C_D_NetworkIdentity& net, C_D_RigidBody& rb) {
            if (net.ownerClientID == clientID && rb.body_created && !rb.is_kinematic && !rb.is_static) {
                float vx = 0.0f, vz = 0.0f;
                if (left)  vx -= speed;
                if (right) vx += speed;
                if (up)    vz -= speed;
                if (down)  vz += speed;
                
                if (vx != 0.0f || vz != 0.0f) {
                    JPH::BodyID jid(rb.jolt_body_id);
                    bi.ActivateBody(jid);
                    JPH::Vec3 curVel = bi.GetLinearVelocity(jid);
                    bi.SetLinearVelocity(jid, JPH::Vec3(vx, curVel.GetY(), vz));
                }
            }
        }
    );
}

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

void Sys_Network::OnFixedUpdate(Registry& reg, float dt) {}

void Sys_Network::OnDestroy(Registry& reg) {
    // 取消订阅，防止析构后依然收到回调
    if (reg.has_ctx<EventBus*>()) {
        reg.ctx<EventBus*>()->unsubscribe<Evt_Net_GameAction>(m_ActionSubID);
    }
    m_Registry = nullptr;

    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.host) {
        enet_host_destroy(resNet.host);
        resNet.host = nullptr;
    }
    enet_deinitialize();
    LOG_INFO("Network System shut down. Sent: " << resNet.packetsSent << ", Received: " << resNet.packetsReceived);
}

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
    SendPacket(resNet, pkt, isServer);
}

} // namespace ECS