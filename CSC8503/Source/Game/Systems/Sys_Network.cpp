/**
 * @file Sys_Network.cpp
 * @brief ENet 网络系统实现。
 *
 * @details
 * 负责网络初始化、收发包、事件桥接，以及客户端与服务端的同步流程。
 */
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
#include "Game/Utils/Assert.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_NetworkIdentity.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_Time.h"
#include "Game/Events/Evt_Net_PeerConnected.h"
#include "Game/Events/Evt_Net_PeerDisconnected.h"
#include "Game/Events/Evt_Net_GameAction.h"
#include "Core/ECS/EventBus.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_InputConfig.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_PlayerInput.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Prefabs/PrefabFactory.h"
#include "Assets.h"
#include "Keyboard.h"
#include <algorithm>
#include <random>
#include <iostream>

namespace ECS {

namespace {

/**
 * @brief 生成权威地图顺序表，用于随机化关卡轮换。
 * @param outSequence 输出的序列缓冲区指针。
 * @param count 缓冲区中可写入的元素个数。
 */
void GenerateAuthoritativeMapSequence(uint8_t* outSequence, size_t count) {
    if (outSequence == nullptr || count == 0) {
        return;
    }

    uint8_t pool[] = { 0, 1, 2, 3, 4 };
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(std::begin(pool), std::end(pool), gen);

    const size_t copyCount = std::min<size_t>(count, std::size(pool));
    for (size_t i = 0; i < copyCount; ++i) {
        outSequence[i] = pool[i];
    }
}

/**
 * @brief 检查当前网络资源中是否存在至少一个已连接的远端对等体。
 * @param resNet 当前帧的网络全局资源。
 * @return 若存在已连接的客户端或服务器对等体则返回 true，否则返回 false。
 */
bool HasAnyConnectedRemotePeer(const Res_Network& resNet) {
    if (resNet.mode == PeerType::CLIENT) {
        return resNet.connected
            && resNet.peer != nullptr
            && resNet.peer->state == ENET_PEER_STATE_CONNECTED;
    }

    if (resNet.mode != PeerType::SERVER || resNet.host == nullptr || resNet.host->peers == nullptr) {
        return false;
    }

    for (size_t i = 0; i < resNet.host->peerCount; ++i) {
        if (resNet.host->peers[i].state == ENET_PEER_STATE_CONNECTED) {
            return true;
        }
    }
    return false;
}

} // namespace

/**
 * @brief 注册网络数据包类型的处理函数映射
 */
void Sys_Network::RegisterHandlers() {
    m_PacketHandlers[SYS_WELCOME]    = &Sys_Network::HandleWelcomePacket;
    m_PacketHandlers[SYNC_TRANSFORM] = &Sys_Network::HandleSyncTransform;
    m_PacketHandlers[SYNC_MATCH_STATE] = &Sys_Network::HandleMatchState;
    m_PacketHandlers[SYNC_MATCH_RESTART] = &Sys_Network::HandleMatchRestart;
    m_PacketHandlers[SYNC_MULTIPLAYER_SETUP] = &Sys_Network::HandleMultiplayerSetup;
    m_PacketHandlers[SYNC_GHOST_TRANSFORM] = &Sys_Network::HandleSyncGhostTransform;
    m_PacketHandlers[CLIENT_INPUT]   = &Sys_Network::HandleClientInput;
    m_PacketHandlers[CLIENT_GHOST_TRANSFORM] = &Sys_Network::HandleClientGhostTransform;
    m_PacketHandlers[CLIENT_MATCH_PROGRESS] = &Sys_Network::HandleClientMatchProgress;
    m_PacketHandlers[CLIENT_MATCH_RESTART_REQUEST] = &Sys_Network::HandleClientMatchRestartRequest;
    m_PacketHandlers[GAME_EVENT]     = &Sys_Network::HandleGameAction;
}

/**
 * @brief 系统初始化阶段，配置事件并初始化 ENet 网络环境
 * @param reg ECS 注册表
 */
void Sys_Network::OnAwake(Registry& reg) {
    RegisterHandlers();
    InitializeEvents(reg);

    auto& resNet = reg.ctx<Res_Network>();
    if (resNet.host != nullptr && CanReuseSession(resNet)) {
        resNet.preserveSessionOnSceneExit = false;
        resNet.remotePeerConnected = HasAnyConnectedRemotePeer(resNet);
        m_TimeSinceLastSend = 0.0f;
        m_InputTimer = 0.0f;
        m_GhostTransformTimer = 0.0f;
        m_LastInputMask = 0u;
        m_LastGhostSourceEntity = Entity::NULL_ENTITY;
        m_LastGhostSentRoundIndex = 0xFF;
        m_LastReportedLocalStageProgress = 0xFF;
        m_LastReportedLocalGameOverReason = 0xFF;
        m_LastBroadcastPhase = 0xFF;
        m_LastBroadcastResult = 0xFF;
        m_LastBroadcastHostStage = 0xFF;
        m_LastBroadcastClientStage = 0xFF;
        m_LastBroadcastRoundIndex = 0xFF;
        m_LastBroadcastGameOverReason = 0xFF;

        if (resNet.mode == PeerType::SERVER && resNet.host->peers != nullptr) {
            uint32_t maxAssignedClientID = 0u;
            for (size_t i = 0; i < resNet.host->peerCount; ++i) {
                const ENetPeer& peer = resNet.host->peers[i];
                if (peer.state != ENET_PEER_STATE_CONNECTED || peer.data == nullptr) {
                    continue;
                }
                const uint32_t clientID = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(peer.data));
                maxAssignedClientID = std::max(maxAssignedClientID, clientID);
            }
            m_NextClientID = std::max<uint32_t>(1u, maxAssignedClientID + 1u);
        } else {
            m_NextClientID = 1u;
        }
        LOG_INFO("[Sys_Network] Reusing existing ENet session for scene transition.");
        return;
    }

    if (resNet.host != nullptr) {
        LOG_WARN("[Sys_Network] Discarding stale ENet session before reinitialization.");
        enet_host_destroy(resNet.host);
        enet_deinitialize();
        ResetNetworkRuntimeState(resNet, true);
    }

    if (enet_initialize() != 0) {
        LOG_ERROR("An error occurred while initializing ENet.");
        return;
    }

    if (resNet.mode == PeerType::SERVER) {
        InitializeServer(resNet);
    } 
    else if (resNet.mode == PeerType::CLIENT) {
        InitializeClient(resNet);
    }
    m_GhostTransformTimer = 0.0f;
    m_LastGhostSourceEntity = Entity::NULL_ENTITY;
    m_LastGhostSentRoundIndex = 0xFF;
}

/**
 * @brief 初始化事件总线并注册网络相关事件的监听器
 * @param reg ECS 注册表
 */
void Sys_Network::InitializeEvents(Registry& reg) {
    // 缓存 Registry 指针供 EventBus 回调使用
    m_Registry = &reg;
    
    // EventBus 由 SceneManager 在场景进入前注入，此处只负责注册监听
    GAME_ASSERT(reg.has_ctx<EventBus*>() && reg.ctx<EventBus*>() != nullptr,
                "[Sys_Network] InitializeEvents: EventBus* not found in registry ctx. "
                "SceneManager must inject EventBus before Sys_Network::OnAwake is called.");
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
    address.port = resNet.serverPort;

    resNet.host = enet_host_create(&address, 4, 2, 0, 0);
    if (resNet.host == nullptr) {
        LOG_ERROR("An error occurred while trying to create an ENet server host.");
        return;
    }
    resNet.localClientID = 0;
    resNet.connected = true;
    resNet.remotePeerConnected = false;
    m_NextClientID = 1; 
    LOG_INFO("Network Server started on port " << resNet.serverPort << ".");
}

/**
 * @brief 将当前节点初始化为客户端，并向指定的服务器发起连接
 * @param resNet 网络资源对象，用于存储客户端主机与连接状态
 */
void Sys_Network::InitializeClient(Res_Network& resNet) {
    resNet.host = enet_host_create(NULL, 1, 2, 0, 0);
    if (resNet.host == nullptr) {
        LOG_ERROR("An error occurred while trying to create an ENet client host.");
        resNet.peer        = nullptr;
        resNet.connected   = false;
        resNet.remotePeerConnected = false;
        resNet.localClientID = UINT32_MAX;
        return;
    }
    // 客户端实际 ID 应在 SYS_WELCOME 包中由 Server 分配。
    // 初始化为最大值表示尚未分配
    resNet.localClientID = UINT32_MAX;

    ENetAddress address;
    if (enet_address_set_host(&address, resNet.serverIP) != 0) {
        LOG_ERROR("Failed to resolve server host: " << resNet.serverIP);
        enet_host_destroy(resNet.host);
        resNet.host        = nullptr;
        resNet.peer        = nullptr;
        resNet.connected   = false;
        resNet.remotePeerConnected = false;
        resNet.localClientID = UINT32_MAX;
        return;
    }
    address.port = resNet.serverPort;

    resNet.peer = enet_host_connect(resNet.host, &address, 2, 0);
    if (resNet.peer == nullptr) {
        LOG_ERROR("No available peers for initiating an ENet connection.");
        enet_host_destroy(resNet.host);
        resNet.host        = nullptr;
        resNet.peer        = nullptr;
        resNet.connected   = false;
        resNet.remotePeerConnected = false;
        resNet.localClientID = UINT32_MAX;
        return;
    }
    m_LastInputMask = 0;
    LOG_INFO("Network Client connecting to " << resNet.serverIP << ":" << resNet.serverPort << "...");
}

/**
 * @brief 每帧更新函数，处理网络事件、收集本地输入并进行状态广播
 * @param reg ECS 注册表
 * @param dt 帧耗时
 */
void Sys_Network::OnUpdate(Registry& reg, float dt) {
    auto& resNet = reg.ctx<Res_Network>();
    if (!resNet.host) return;
    ResetFrameFlags(reg);

    // 1. 处理所有传入的网络事件
    ProcessNetworkEvents(reg, resNet);

    if (resNet.peer != nullptr) {
        resNet.rtt = resNet.peer->roundTripTime;
    } else {
        resNet.rtt = 0;
    }
    if (reg.has_ctx<Res_GameState>()) {
        reg.ctx<Res_GameState>().networkPing = resNet.rtt;
    }

    if (!resNet.connected) return;

    RefreshRemoteGhostEntity(reg, resNet);

    // 2. 处理本地输入（客户端发包，服务端直接驱动物理）
    HandleLocalInput(reg, resNet);

    if (resNet.mode == PeerType::CLIENT) {
        UpdateClientMatchProgress(reg, resNet);
    }
    SyncGhostTransforms(reg, resNet);

    ProcessMatchRestartRequest(reg, resNet);

    if (resNet.mode == PeerType::SERVER) {
        BroadcastMatchStateIfDirty(reg, resNet);
    }

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
                    resNet.remotePeerConnected = true;
                    uint32_t newClientID = m_NextClientID++;
                    event.peer->data = (void*)(uintptr_t)newClientID;
                    if (reg.has_ctx<EventBus*>()) {
                        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerConnected>({ newClientID });
                    }
                    
                    // 发送 Welcome 数据包分配 ID 给客户端
                    Net_Packet_Welcome welcomePkt;
                    welcomePkt.type = SYS_WELCOME;
                    welcomePkt.clientID = newClientID;
                    welcomePkt.timestamp = 0;
                    // 使用 SendPacket 助手函数，内部会处理 flags 映射
                    SendPacket(resNet, welcomePkt, NetTarget::Single, NetDelivery::Reliable, event.peer);
                    if (reg.has_ctx<Res_GameState>()) {
                        auto& gs = reg.ctx<Res_GameState>();
                        if (gs.isMultiplayer && gs.matchPhase == MatchPhase::WaitingForPeer) {
                            gs.matchPhase = MatchPhase::Running;
                            gs.matchJustStarted = true;
                        }
                    }
                    if (resNet.multiplayerMode == MultiplayerMode::SameMapGhostRace) {
                        BroadcastMultiplayerSetup(reg, resNet, event.peer);
                        if (reg.has_ctx<Res_UIState>()) {
                            auto& ui = reg.ctx<Res_UIState>();
                            if (resNet.bootstrapSceneActive && ui.pendingSceneRequest == SceneRequest::None) {
                                ui.pendingSceneRequest = SceneRequest::LaunchMultiplayerMatch;
                            }
                        }
                    }
                    BroadcastMatchStateIfDirty(reg, resNet, true);
                } else {
                    resNet.connected = true;
                    resNet.remotePeerConnected = true;
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
                uint32_t disconnectedID = (resNet.mode == PeerType::SERVER) ? GetClientID(event) : 0u;
                LOG_INFO("Peer disconnected. ID: " << disconnectedID);
                if (reg.has_ctx<EventBus*>()) {
                    reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerDisconnected>({ disconnectedID });
                }
                if (reg.has_ctx<Res_GameState>()) {
                    auto& gs = reg.ctx<Res_GameState>();
                    if (gs.isMultiplayer) {
                        gs.matchPhase = MatchPhase::Finished;
                        gs.matchResult = MatchResult::Disconnected;
                        gs.matchJustFinished = true;
                        gs.isGameOver = true;
                        gs.gameOverReason = GameOverReason::None;
                    }
                }
                UpdateMatchUIState(reg);
                if (resNet.mode == PeerType::CLIENT) {
                    resNet.connected = false;
                    resNet.remotePeerConnected = false;
                } else {
                    resNet.remotePeerConnected = HasAnyConnectedRemotePeer(resNet);
                    BroadcastMatchStateIfDirty(reg, resNet, true);
                }
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
    resNet.remotePeerConnected = true;
    LOG_INFO("Received SYS_WELCOME. Assigned Client ID: " << resNet.localClientID);
    
    // 此时已经收到真实的 ClientID，可以在这里抛出事件通知其他系统进行玩家实体的创建等逻辑
    if (reg.has_ctx<EventBus*>()) {
        reg.ctx<EventBus*>()->publish_deferred<Evt_Net_PeerConnected>({ pkt->clientID });
    }
}

/**
 * @brief 处理服务端广播的多人比赛状态快照。
 * @details 客户端会保留尚未被服务端确认的本地终局状态，避免旧的 Running 快照覆盖本地终局。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleMatchState(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::CLIENT || !reg.has_ctx<Res_GameState>()) return;

    auto* pkt = GetPacketData<Net_Packet_MatchState>(event);
    if (!pkt) return;

    auto& gs = reg.ctx<Res_GameState>();
    const MatchPhase previousPhase = gs.matchPhase;
    const uint8_t previousLocalProgress = gs.localStageProgress;
    const uint8_t previousOpponentProgress = gs.opponentStageProgress;
    const uint8_t pendingLocalStageProgress = gs.localStageProgress;
    const MatchPhase incomingPhase = static_cast<MatchPhase>(pkt->matchPhase);
    const MatchResult incomingResult = ToClientPerspective(static_cast<MatchResult>(pkt->matchResult));

    gs.matchPhase = incomingPhase;
    gs.matchResult = incomingResult;
    gs.authoritativeMatchFinished = (incomingPhase == MatchPhase::Finished);
    gs.localStageProgress = std::max(ClampStageProgress(pkt->clientStageProgress), pendingLocalStageProgress);
    gs.opponentStageProgress = ClampStageProgress(pkt->hostStageProgress);
    gs.currentRoundIndex = std::min<uint8_t>(pkt->currentRoundIndex, kMultiplayerStageCount - 1);
    gs.localProgress = gs.localStageProgress;
    gs.opponentProgress = gs.opponentStageProgress;
    gs.matchJustStarted = (previousPhase != MatchPhase::Running && gs.matchPhase == MatchPhase::Running);
    gs.matchJustFinished = (previousPhase != MatchPhase::Finished && gs.matchPhase == MatchPhase::Finished);
    gs.roundJustAdvanced = (previousLocalProgress != gs.localStageProgress)
        || (previousOpponentProgress != gs.opponentStageProgress);

    if (incomingPhase == MatchPhase::Finished) {
        gs.gameOverReason = ToGameOverReason(ComputeGameOverReasonForResult(gs.matchResult));
        gs.isGameOver = true;
    } else {
        gs.gameOverReason = GameOverReason::None;
        gs.isGameOver = false;
    }

    UpdateMatchUIState(reg);
}

/**
 * @brief 处理服务端广播的多人重开指令。
 * @details 收到后由双方统一走 `RestartLevel`，避免本地 UI 直接重开造成状态分叉。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleMatchRestart(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    auto* pkt = GetPacketData<Net_Packet_MatchRestart>(event);
    if (!pkt || !reg.has_ctx<Res_UIState>()) return;

    ResetMatchStateForRestart(reg);
    resNet.multiplayerMode = static_cast<MultiplayerMode>(pkt->multiplayerMode);
    if (resNet.multiplayerMode == MultiplayerMode::SameMapGhostRace) {
        ApplyAuthoritativeMapSequence(reg, pkt->mapSequence, pkt->currentRoundIndex);
        resNet.matchSetupReceived = true;
    }
    auto& ui = reg.ctx<Res_UIState>();
    ui.multiplayerRetryRequested = false;
    ui.pendingSceneRequest = SceneRequest::RestartLevel;
}

/**
 * @brief 处理服务端下发的同图模式配置与地图序列。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleMultiplayerSetup(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::CLIENT) return;

    auto* pkt = GetPacketData<Net_Packet_MultiplayerSetup>(event);
    if (!pkt) return;

    resNet.multiplayerMode = static_cast<MultiplayerMode>(pkt->multiplayerMode);
    ApplyAuthoritativeMapSequence(reg, pkt->mapSequence, pkt->currentRoundIndex);
    resNet.matchSetupReceived = true;

    if (resNet.multiplayerMode == MultiplayerMode::SameMapGhostRace
        && resNet.bootstrapSceneActive
        && reg.has_ctx<Res_UIState>()) {
        auto& ui = reg.ctx<Res_UIState>();
        if (ui.pendingSceneRequest == SceneRequest::None) {
            ui.pendingSceneRequest = SceneRequest::LaunchMultiplayerMatch;
        }
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
            
            //抛弃服务器发来的时间戳，改用客户端收到该包的本地时间戳。
            float localReceiveTimeMs = reg.ctx<Res_Time>().totalTime * 1000.0f;
            InterpBuffer_AddSnapshot(buffer,
                NCL::Maths::Vector3(pkt->pos[0], pkt->pos[1], pkt->pos[2]),
                NCL::Maths::Quaternion(pkt->rot[0], pkt->rot[1], pkt->rot[2], pkt->rot[3]),
                localReceiveTimeMs
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
 * @brief 处理客户端上报的幽灵位姿，并在 Host 本地更新远端幽灵。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleClientGhostTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::SERVER || resNet.multiplayerMode != MultiplayerMode::SameMapGhostRace) {
        return;
    }

    auto* pkt = GetPacketData<Net_Packet_GhostTransform>(event);
    if (!pkt) return;
    const bool roundChanged = !resNet.remoteGhostSnapshotValid
        || resNet.remoteGhostSnapshotRoundIndex != pkt->currentRoundIndex;
    CacheRemoteGhostSnapshot(resNet, *pkt);
    ApplyCachedRemoteGhostSnapshot(reg, resNet, roundChanged);
}

/**
 * @brief 处理客户端上报的阶段推进和终局状态。
 * @details 服务端收到后会收口权威比赛结果，并在状态变化后广播最新快照。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleClientMatchProgress(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::SERVER || !reg.has_ctx<Res_GameState>()) return;

    auto* pkt = GetPacketData<Net_Packet_ClientMatchProgress>(event);
    if (!pkt) return;

    auto& gs = reg.ctx<Res_GameState>();
    const uint8_t previousOpponentProgress = gs.opponentStageProgress;
    gs.opponentStageProgress = ClampStageProgress(pkt->stageProgress);
    gs.currentRoundIndex = ComputeCurrentRoundIndex(gs.localStageProgress, gs.opponentStageProgress);
    gs.opponentProgress = gs.opponentStageProgress;
    gs.roundJustAdvanced = (previousOpponentProgress != gs.opponentStageProgress);
    gs.remoteTerminalState = static_cast<MultiplayerTerminalState>(pkt->terminalState);
    gs.remoteTerminalReason = pkt->terminalReason;

    if (gs.matchPhase == MatchPhase::WaitingForPeer || gs.matchPhase == MatchPhase::Starting) {
        gs.matchPhase = MatchPhase::Running;
        gs.matchJustStarted = true;
    }

    if (pkt->reportedFinished != 0u || IsFinalTerminalState(gs.remoteTerminalState)) {
        LOG_INFO("[Sys_Network] Server received client terminal state: stage="
                 << (int)pkt->stageProgress << " finished=" << (int)pkt->reportedFinished
                 << " terminalState=" << (int)pkt->terminalState
                 << " reason=" << (int)pkt->terminalReason);
    }

    ApplyMatchResult(gs);
    BroadcastMatchStateIfDirty(reg, resNet, true);
}

/**
 * @brief 处理客户端发起的多人 Retry 请求。
 * @details 仅当服务端权威比赛已结束时才允许广播重开，避免中途收到异常包强制重置战局。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleClientMatchRestartRequest(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::SERVER
        || !GetPacketData<Net_Packet_MatchRestart>(event)
        || !reg.has_ctx<Res_GameState>()) {
        return;
    }

    auto& gs = reg.ctx<Res_GameState>();
    if (!gs.isMultiplayer || gs.matchPhase != MatchPhase::Finished) {
        LOG_WARN("[Sys_Network] Ignored premature client restart request while matchPhase="
                 << static_cast<int>(gs.matchPhase));
        return;
    }

    BroadcastMatchRestart(reg, resNet);
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
 * @brief 清空仅在当前帧内生效的比赛状态边沿标记。
 * @param reg ECS 注册表
 */
void Sys_Network::ResetFrameFlags(Registry& reg) {
    if (!reg.has_ctx<Res_GameState>()) return;

    auto& gs = reg.ctx<Res_GameState>();
    gs.roundJustAdvanced = false;
    gs.matchJustStarted = false;
    gs.matchJustFinished = false;
}

/**
 * @brief 收集本地玩家输入，客户端会将其打包发送给服务端，服务端则直接应用于本地控制的实体
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::HandleLocalInput(Registry& reg, Res_Network& resNet) {
    if (!reg.has_ctx<Res_Input>()) return;
    auto& input = reg.ctx<Res_Input>();

    Res_InputConfig defaultCfg;
    const auto& cfg = reg.has_ctx<Res_InputConfig>() ? reg.ctx<Res_InputConfig>() : defaultCfg;

    uint32_t currentMask = 0;
    if (input.keyStates[cfg.keyChatUp])    currentMask |= PlayerInputFlags::Up;
    if (input.keyStates[cfg.keyChatDown])  currentMask |= PlayerInputFlags::Down;
    if (input.keyStates[cfg.keyChatLeft])  currentMask |= PlayerInputFlags::Left;
    if (input.keyStates[cfg.keyChatRight]) currentMask |= PlayerInputFlags::Right;
    
    // --- 1. Client：收集输入并发送给 Server ---
    if (resNet.mode == PeerType::CLIENT && resNet.peer != nullptr) {
        Net_Packet_ClientInput pkt;
        pkt.type = CLIENT_INPUT;
        pkt.timestamp = (uint32_t)(reg.ctx<Res_Time>().totalTime * 1000.0f);
        pkt.buttonMask = currentMask;
        bool isMoving = (currentMask != 0);
        bool stateChanged = (currentMask != m_LastInputMask);
        m_InputTimer += reg.ctx<Res_Time>().deltaTime;

        if (stateChanged) {
            // 状态改变时（无论是按下新键还是松开），发送一次可靠包
            SendPacket(resNet, pkt, NetTarget::Single, NetDelivery::Reliable);
            m_InputTimer = 0.0f; // 重置计时器
        } else if (isMoving && m_InputTimer >= m_SendRate) {
            // 持续按键时，按照设定频率发送不可靠包
            SendPacket(resNet, pkt, NetTarget::Single, NetDelivery::Unreliable);
            m_InputTimer -= m_SendRate;
            if (m_InputTimer > m_SendRate) m_InputTimer = 0.0f; // 防止螺旋
        }
        m_LastInputMask = currentMask;
    }

    // --- 2. Server：处理主机本地玩家的输入 ---
    if (resNet.mode == PeerType::SERVER) {
        UpdatePlayerInput(reg, resNet.localClientID, currentMask);
    }
}

/**
 * @brief 同步本地玩家位姿到远端幽灵显示通道。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::SyncGhostTransforms(Registry& reg, Res_Network& resNet) {
    const bool eligibleMode = resNet.multiplayerMode == MultiplayerMode::SameMapGhostRace;
    const bool hasGameState = reg.has_ctx<Res_GameState>();
    const bool hasTime = reg.has_ctx<Res_Time>();
    const bool localPlayerAssigned = Entity::IsValid(resNet.localPlayerEntity);
    const bool localPlayerValid = localPlayerAssigned && reg.Valid(resNet.localPlayerEntity);
    const bool localPlayerHasTransform = localPlayerValid && reg.Has<C_D_Transform>(resNet.localPlayerEntity);

    if (!eligibleMode
        || !hasGameState
        || !hasTime
        || resNet.bootstrapSceneActive
        || !localPlayerAssigned
        || !localPlayerValid
        || !localPlayerHasTransform) {
        return;
    }

    m_GhostTransformTimer += reg.ctx<Res_Time>().deltaTime;
    if (m_GhostTransformTimer < m_SendRate) {
        return;
    }
    m_GhostTransformTimer -= m_SendRate;
    if (m_GhostTransformTimer > m_SendRate) {
        m_GhostTransformTimer = 0.0f;
    }

    const auto& tf = reg.Get<C_D_Transform>(resNet.localPlayerEntity);
    Net_Packet_GhostTransform pkt;
    pkt.timestamp = static_cast<uint32_t>(reg.ctx<Res_Time>().totalTime * 1000.0f);
    pkt.pos[0] = tf.position.x; pkt.pos[1] = tf.position.y; pkt.pos[2] = tf.position.z;
    pkt.rot[0] = tf.rotation.x; pkt.rot[1] = tf.rotation.y; pkt.rot[2] = tf.rotation.z; pkt.rot[3] = tf.rotation.w;
    pkt.currentRoundIndex = reg.ctx<Res_GameState>().currentRoundIndex;
    const bool forceReliableSeed = (m_LastGhostSourceEntity != resNet.localPlayerEntity)
        || (m_LastGhostSentRoundIndex != pkt.currentRoundIndex);
    m_LastGhostSourceEntity = resNet.localPlayerEntity;
    m_LastGhostSentRoundIndex = pkt.currentRoundIndex;

    if (resNet.mode == PeerType::SERVER) {
        pkt.type = SYNC_GHOST_TRANSFORM;
        SendPacket(resNet,
            pkt,
            NetTarget::Broadcast,
            forceReliableSeed ? NetDelivery::Reliable : NetDelivery::Unreliable);
    } else if (resNet.mode == PeerType::CLIENT && resNet.peer != nullptr) {
        pkt.type = CLIENT_GHOST_TRANSFORM;
        SendPacket(resNet,
            pkt,
            NetTarget::Single,
            forceReliableSeed ? NetDelivery::Reliable : NetDelivery::Unreliable);
    }
}

/**
 * @brief 由客户端向服务端上报本地阶段进度和待确认终局状态。
 * @details 当客户端已经本地终局但尚未收到权威 `Finished` 时，会持续重发，避免单次丢包卡死。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::UpdateClientMatchProgress(Registry& reg, Res_Network& resNet) {
    if (resNet.mode != PeerType::CLIENT || resNet.peer == nullptr || !reg.has_ctx<Res_GameState>()) return;

    auto& gs = reg.ctx<Res_GameState>();
    if (!gs.isMultiplayer) return;

    const uint8_t localStageProgress = ClampStageProgress(gs.localStageProgress);
    const MultiplayerTerminalState localTerminalState = GetLocalTerminalState(gs);
    const uint8_t localTerminalReason = GetLocalTerminalReason(gs);
    const bool hasPendingTerminalState =
        IsFinalTerminalState(localTerminalState)
        && gs.matchPhase != MatchPhase::Finished;
    if (!hasPendingTerminalState
        && m_LastReportedLocalStageProgress == localStageProgress
        && m_LastReportedLocalGameOverReason == localTerminalReason) {
        return;
    }

    Net_Packet_ClientMatchProgress pkt;
    pkt.type = CLIENT_MATCH_PROGRESS;
    pkt.timestamp = static_cast<uint32_t>(reg.ctx<Res_Time>().totalTime * 1000.0f);
    pkt.stageProgress = localStageProgress;
    pkt.currentRoundIndex = std::min<uint8_t>(gs.currentRoundIndex, kMultiplayerStageCount - 1);
    pkt.reportedFinished = hasPendingTerminalState ? 1u : 0u;
    pkt.terminalState = static_cast<uint8_t>(localTerminalState);
    pkt.terminalReason = localTerminalReason;

    if (pkt.reportedFinished != 0u || pkt.terminalReason != 0u) {
        LOG_INFO("[Sys_Network] Client reporting terminal state: stage="
                 << (int)pkt.stageProgress << " finished=" << (int)pkt.reportedFinished
                 << " terminalState=" << (int)pkt.terminalState
                 << " reason=" << (int)pkt.terminalReason
                 << " phase=" << (int)gs.matchPhase);
    }

    SendPacket(resNet, pkt, NetTarget::Single, NetDelivery::Reliable);
    m_LastReportedLocalStageProgress = localStageProgress;
    m_LastReportedLocalGameOverReason = localTerminalReason;
}

/**
 * @brief 处理本地 UI 发起的多人 Retry 请求。
 * @details Host 会直接广播重开；Client 会先向 Host 发请求，由 Host 统一裁决并广播。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::ProcessMatchRestartRequest(Registry& reg, Res_Network& resNet) {
    if (!reg.has_ctx<Res_UIState>() || !reg.has_ctx<Res_GameState>()) return;

    auto& ui = reg.ctx<Res_UIState>();
    auto& gs = reg.ctx<Res_GameState>();
    if (!ui.multiplayerRetryRequested || !gs.isMultiplayer) return;

    if (gs.matchResult == MatchResult::Disconnected || !resNet.connected) {
        ui.multiplayerRetryRequested = false;
        ui.pendingSceneRequest = SceneRequest::RestartLevel;
        return;
    }

    if (gs.matchPhase != MatchPhase::Finished) {
        ui.multiplayerRetryRequested = false;
        return;
    }

    if (resNet.mode == PeerType::SERVER) {
        BroadcastMatchRestart(reg, resNet);
        return;
    }

    if (resNet.mode == PeerType::CLIENT && resNet.peer != nullptr) {
        Net_Packet_MatchRestart pkt;
        pkt.type = CLIENT_MATCH_RESTART_REQUEST;
        pkt.timestamp = static_cast<uint32_t>(reg.ctx<Res_Time>().totalTime * 1000.0f);
        SendPacket(resNet, pkt, NetTarget::Single, NetDelivery::Reliable);
        ui.multiplayerRetryRequested = false;
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
 * @brief 当服务端权威比赛状态发生变化时广播最新快照。
 * @details 同时负责规范化阶段值、推导轮次索引，并把多人结果桥接回现有 UI/GameOver 状态。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param force 是否忽略脏检查强制广播
 */
void Sys_Network::BroadcastMatchStateIfDirty(Registry& reg, Res_Network& resNet, bool force) {
    if (resNet.mode != PeerType::SERVER || !reg.has_ctx<Res_GameState>()) return;

    auto& gs = reg.ctx<Res_GameState>();
    if (!gs.isMultiplayer) return;

    gs.localStageProgress = ClampStageProgress(gs.localStageProgress);
    gs.opponentStageProgress = ClampStageProgress(gs.opponentStageProgress);
    gs.currentRoundIndex = ComputeCurrentRoundIndex(gs.localStageProgress, gs.opponentStageProgress);
    gs.localProgress = gs.localStageProgress;
    gs.opponentProgress = gs.opponentStageProgress;
    ApplyMatchResult(gs);
    UpdateMatchUIState(reg);

    const uint8_t phase = static_cast<uint8_t>(gs.matchPhase);
    const uint8_t result = static_cast<uint8_t>(gs.matchResult);
    const uint8_t hostStage = gs.localStageProgress;
    const uint8_t clientStage = gs.opponentStageProgress;
    const uint8_t roundIndex = gs.currentRoundIndex;
    const uint8_t gameOverReason = ComputeGameOverReasonForResult(gs.matchResult);
    gs.gameOverReason = ToGameOverReason(gameOverReason);

    const bool dirty = force
        || m_LastBroadcastPhase != phase
        || m_LastBroadcastResult != result
        || m_LastBroadcastHostStage != hostStage
        || m_LastBroadcastClientStage != clientStage
        || m_LastBroadcastRoundIndex != roundIndex
        || m_LastBroadcastGameOverReason != gameOverReason;

    if (!dirty) return;

    Net_Packet_MatchState pkt;
    pkt.type = SYNC_MATCH_STATE;
    pkt.timestamp = static_cast<uint32_t>(reg.ctx<Res_Time>().totalTime * 1000.0f);
    pkt.matchPhase = phase;
    pkt.matchResult = result;
    pkt.hostStageProgress = hostStage;
    pkt.clientStageProgress = clientStage;
    pkt.currentRoundIndex = roundIndex;
    pkt.gameOverReason = gameOverReason;
    if (phase == static_cast<uint8_t>(MatchPhase::Finished)) {
        LOG_INFO("[Sys_Network] Server broadcasting finished match state: result="
                 << (int)result << " hostStage=" << (int)hostStage
                 << " clientStage=" << (int)clientStage
                 << " reason=" << (int)gameOverReason);
    }
    SendPacket(resNet, pkt, NetTarget::Broadcast, NetDelivery::Reliable);

    m_LastBroadcastPhase = phase;
    m_LastBroadcastResult = result;
    m_LastBroadcastHostStage = hostStage;
    m_LastBroadcastClientStage = clientStage;
    m_LastBroadcastRoundIndex = roundIndex;
    m_LastBroadcastGameOverReason = gameOverReason;
}

/**
 * @brief 处理服务端广播的远端幽灵位姿。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param event ENet 事件对象
 */
void Sys_Network::HandleSyncGhostTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event) {
    if (resNet.mode != PeerType::CLIENT || resNet.multiplayerMode != MultiplayerMode::SameMapGhostRace) {
        return;
    }

    auto* pkt = GetPacketData<Net_Packet_GhostTransform>(event);
    if (!pkt) return;
    const bool roundChanged = !resNet.remoteGhostSnapshotValid
        || resNet.remoteGhostSnapshotRoundIndex != pkt->currentRoundIndex;
    CacheRemoteGhostSnapshot(resNet, *pkt);
    ApplyCachedRemoteGhostSnapshot(reg, resNet, roundChanged);
}

/**
 * @brief 由服务端向目标对端广播同图模式配置与权威地图序列。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @param explicitPeer 为空时广播给所有客户端
 */
void Sys_Network::BroadcastMultiplayerSetup(Registry& reg, Res_Network& resNet, ENetPeer* explicitPeer) {
    if (resNet.mode != PeerType::SERVER || !reg.has_ctx<Res_UIState>()) {
        return;
    }

    auto& ui = reg.ctx<Res_UIState>();
    if (!ui.mapSequenceGenerated) {
        GenerateAuthoritativeMapSequence(ui.mapSequence, Res_UIState::MAP_SEQUENCE_LENGTH);
        ui.mapSequenceGenerated = true;
        ui.mapSequenceIndex = 0;
    }

    Net_Packet_MultiplayerSetup pkt;
    pkt.type = SYNC_MULTIPLAYER_SETUP;
    pkt.timestamp = reg.has_ctx<Res_Time>()
        ? static_cast<uint32_t>(reg.ctx<Res_Time>().totalTime * 1000.0f)
        : 0u;
    pkt.multiplayerMode = static_cast<uint8_t>(resNet.multiplayerMode);
    for (int i = 0; i < Res_UIState::MAP_SEQUENCE_LENGTH; ++i) {
        pkt.mapSequence[i] = ui.mapSequence[i];
    }
    pkt.currentRoundIndex = ui.mapSequenceIndex;

    if (explicitPeer != nullptr) {
        SendPacket(resNet, pkt, NetTarget::Single, NetDelivery::Reliable, explicitPeer);
    } else {
        SendPacket(resNet, pkt, NetTarget::Broadcast, NetDelivery::Reliable);
    }
}

/**
 * @brief 由服务端广播多人重开指令，并让本地 UI 同步进入重开流程。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 */
void Sys_Network::BroadcastMatchRestart(Registry& reg, Res_Network& resNet) {
    if (!reg.has_ctx<Res_UIState>()) return;

    ResetMatchStateForRestart(reg);
    auto& ui = reg.ctx<Res_UIState>();

    if (resNet.multiplayerMode == MultiplayerMode::SameMapGhostRace) {
        GenerateAuthoritativeMapSequence(ui.mapSequence, Res_UIState::MAP_SEQUENCE_LENGTH);
        ui.mapSequenceGenerated = true;
        ui.mapSequenceIndex = 0;
        resNet.matchSetupReceived = true;
    }

    Net_Packet_MatchRestart pkt;
    pkt.type = SYNC_MATCH_RESTART;
    pkt.timestamp = reg.has_ctx<Res_Time>()
        ? static_cast<uint32_t>(reg.ctx<Res_Time>().totalTime * 1000.0f)
        : 0u;
    pkt.multiplayerMode = static_cast<uint8_t>(resNet.multiplayerMode);
    for (int i = 0; i < Res_UIState::MAP_SEQUENCE_LENGTH; ++i) {
        pkt.mapSequence[i] = ui.mapSequence[i];
    }
    pkt.currentRoundIndex = 0u;
    SendPacket(resNet, pkt, NetTarget::Broadcast, NetDelivery::Reliable);

    ui.multiplayerRetryRequested = false;
    ui.pendingSceneRequest = SceneRequest::RestartLevel;
}

/**
 * @brief 将多人比赛状态重置为新一局的初始状态。
 * @details 在切场景前清掉旧的 Finished/Result/Progress，避免旧战局状态继续广播并把双方拉回 GameOver。
 * @param reg ECS 注册表
 */
void Sys_Network::ResetMatchStateForRestart(Registry& reg) {
    if (reg.has_ctx<Res_GameState>()) {
        auto& gs = reg.ctx<Res_GameState>();
        if (gs.isMultiplayer) {
            gs.matchPhase = MatchPhase::Running;
            gs.matchResult = MatchResult::None;
            gs.authoritativeMatchFinished = false;
            gs.currentRoundIndex = 0;
            gs.localStageProgress = 0;
            gs.opponentStageProgress = 0;
            gs.localProgress = 0;
            gs.opponentProgress = 0;
            gs.roundJustAdvanced = false;
            gs.matchJustStarted = true;
            gs.matchJustFinished = false;
            gs.localTerminalState = MultiplayerTerminalState::None;
            gs.remoteTerminalState = MultiplayerTerminalState::None;
            gs.localTerminalReason = 0u;
            gs.remoteTerminalReason = 0u;
            gs.isGameOver = false;
            gs.gameOverReason = GameOverReason::None;
            gs.gameOverTime = 0.0f;
            gs.countdownActive = false;
            gs.countdownTimer = gs.countdownMax;
        }
    }

    m_LastReportedLocalStageProgress = 0xFF;
    m_LastReportedLocalGameOverReason = 0xFF;
    m_LastBroadcastPhase = 0xFF;
    m_LastBroadcastResult = 0xFF;
    m_LastBroadcastHostStage = 0xFF;
    m_LastBroadcastClientStage = 0xFF;
    m_LastBroadcastRoundIndex = 0xFF;
    m_LastBroadcastGameOverReason = 0xFF;
}

/**
 * @brief 将服务端权威地图序列写入 UI 会话状态。
 * @param reg ECS 注册表
 * @param mapSequence 三关地图序列
 * @param roundIndex 当前轮次索引
 */
void Sys_Network::ApplyAuthoritativeMapSequence(Registry& reg,
                                                const uint8_t* mapSequence,
                                                uint8_t roundIndex) {
    if (!reg.has_ctx<Res_UIState>() || mapSequence == nullptr) {
        return;
    }

    auto& ui = reg.ctx<Res_UIState>();
    for (int i = 0; i < Res_UIState::MAP_SEQUENCE_LENGTH; ++i) {
        ui.mapSequence[i] = mapSequence[i];
    }
    ui.mapSequenceGenerated = true;
    ui.mapSequenceIndex = std::min<uint8_t>(roundIndex, Res_UIState::MAP_SEQUENCE_LENGTH - 1);
}

/**
 * @brief 确保当前场景存在远端幽灵显示实体。
 * @param reg ECS 注册表
 * @param resNet 网络资源对象
 * @return 幽灵实体 ID；失败时返回 NULL_ENTITY
 */
EntityID Sys_Network::EnsureRemoteGhostEntity(Registry& reg, Res_Network& resNet) {
    if (Entity::IsValid(resNet.remoteGhostEntity)
        && reg.Valid(resNet.remoteGhostEntity)
        && reg.Has<C_D_Transform>(resNet.remoteGhostEntity)
        && reg.Has<C_D_InterpBuffer>(resNet.remoteGhostEntity)) {
        return resNet.remoteGhostEntity;
    }

    ECS::AssetManager::Instance().Init();
    ECS::MeshHandle cubeMesh = ECS::AssetManager::Instance().LoadMesh(NCL::Assets::MESHDIR + "cube.obj");
    const EntityID ghost = PrefabFactory::CreateGhostPlayer(
        reg, cubeMesh, NCL::Maths::Vector3(0.0f, 1.5f, 0.0f));
    if (!Entity::IsValid(ghost)) {
        return Entity::NULL_ENTITY;
    }

    resNet.remoteGhostEntity = ghost;
    return ghost;
}

void Sys_Network::CacheRemoteGhostSnapshot(Res_Network& resNet, const Net_Packet_GhostTransform& pkt) {
    resNet.remoteGhostSnapshotValid = true;
    resNet.remoteGhostSnapshotRoundIndex = pkt.currentRoundIndex;
    std::copy(std::begin(pkt.pos), std::end(pkt.pos), std::begin(resNet.remoteGhostSnapshotPos));
    std::copy(std::begin(pkt.rot), std::end(pkt.rot), std::begin(resNet.remoteGhostSnapshotRot));
}

void Sys_Network::ApplyCachedRemoteGhostSnapshot(Registry& reg,
                                                 Res_Network& resNet,
                                                 bool resetInterpolationBuffer) {
    if (resNet.multiplayerMode != MultiplayerMode::SameMapGhostRace
        || !resNet.remoteGhostSnapshotValid) {
        HideRemoteGhostEntity(reg, resNet);
        return;
    }

    if (!reg.has_ctx<Res_GameState>()) {
        HideRemoteGhostEntity(reg, resNet);
        return;
    }

    const auto& gs = reg.ctx<Res_GameState>();
    if (resNet.remoteGhostSnapshotRoundIndex != gs.currentRoundIndex) {
        HideRemoteGhostEntity(reg, resNet);
        return;
    }

    EntityID ghost = EnsureRemoteGhostEntity(reg, resNet);
    if (!Entity::IsValid(ghost) || !reg.Valid(ghost) || !reg.Has<C_D_Transform>(ghost)) {
        return;
    }

    const NCL::Maths::Vector3 ghostPos(
        resNet.remoteGhostSnapshotPos[0],
        resNet.remoteGhostSnapshotPos[1],
        resNet.remoteGhostSnapshotPos[2]);
    const NCL::Maths::Quaternion ghostRot(
        resNet.remoteGhostSnapshotRot[0],
        resNet.remoteGhostSnapshotRot[1],
        resNet.remoteGhostSnapshotRot[2],
        resNet.remoteGhostSnapshotRot[3]);

    if (reg.Has<C_D_InterpBuffer>(ghost)) {
        auto& buffer = reg.Get<C_D_InterpBuffer>(ghost);
        if (resetInterpolationBuffer) {
            buffer = C_D_InterpBuffer{};
        }

        const float localReceiveTimeMs = reg.has_ctx<Res_Time>()
            ? reg.ctx<Res_Time>().totalTime * 1000.0f
            : 0.0f;
        InterpBuffer_AddSnapshot(buffer, ghostPos, ghostRot, localReceiveTimeMs);
    }

    auto& tf = reg.Get<C_D_Transform>(ghost);
    tf.position = ghostPos;
    tf.rotation = ghostRot;
}

void Sys_Network::RefreshRemoteGhostEntity(Registry& reg, Res_Network& resNet) {
    if (!resNet.connected
        || resNet.multiplayerMode != MultiplayerMode::SameMapGhostRace
        || !resNet.remotePeerConnected
        || !resNet.remoteGhostSnapshotValid
        || resNet.bootstrapSceneActive) {
        HideRemoteGhostEntity(reg, resNet);
        return;
    }

    ApplyCachedRemoteGhostSnapshot(reg, resNet, false);
}

void Sys_Network::HideRemoteGhostEntity(Registry& reg, Res_Network& resNet) {
    if (Entity::IsValid(resNet.remoteGhostEntity) && reg.Valid(resNet.remoteGhostEntity)) {
        reg.Destroy(resNet.remoteGhostEntity);
    }
    resNet.remoteGhostEntity = Entity::NULL_ENTITY;
}

/**
 * @brief 将多人比赛结束态映射到 `UIScreen::GameOver`。
 * @param reg ECS 注册表
 */
void Sys_Network::UpdateMatchUIState(Registry& reg) {
    if (!reg.has_ctx<Res_GameState>() || !reg.has_ctx<Res_UIState>()) return;

    auto& gs = reg.ctx<Res_GameState>();
    if (!gs.isMultiplayer || gs.matchPhase != MatchPhase::Finished) return;

    auto& ui = reg.ctx<Res_UIState>();
    const bool isLeavingCurrentScene =
        ui.pendingSceneRequest != SceneRequest::None
        || ui.transitionSceneRequest != SceneRequest::None
        || ui.transitionActive
        || ui.sceneRequestDispatched
        || ui.activeScreen == UIScreen::Loading
        || ui.activeScreen == UIScreen::None;
    if (isLeavingCurrentScene) {
        return;
    }

    if (ui.activeScreen != UIScreen::GameOver) {
        ui.previousScreen = ui.activeScreen;
        ui.activeScreen = UIScreen::GameOver;
        ui.gameOverSelectedIndex = 0;
    }
}

/**
 * @brief 根据当前双方阶段数和本地终局原因收口比赛结果。
 * @details 该函数只运行在权威状态收口路径，确保 `matchPhase/matchResult/isGameOver` 保持一致。
 * @param gs 当前比赛状态资源
 */
void Sys_Network::ApplyMatchResult(Res_GameState& gs) {
    const bool hostFinishedBySuccess = gs.localTerminalState == MultiplayerTerminalState::FinishedVictory;
    const bool clientFinishedBySuccess = gs.remoteTerminalState == MultiplayerTerminalState::FinishedVictory;
    const bool hostFinishedByFailure =
        IsFinalTerminalState(gs.localTerminalState) && !hostFinishedBySuccess;
    const bool clientFinishedByFailure =
        IsFinalTerminalState(gs.remoteTerminalState) && !clientFinishedBySuccess;

    const bool hostFinished = hostFinishedBySuccess || hostFinishedByFailure;
    const bool clientFinished = clientFinishedBySuccess || clientFinishedByFailure;

    if (!hostFinished && !clientFinished) {
        if (gs.matchPhase != MatchPhase::Finished && gs.matchPhase != MatchPhase::WaitingForPeer) {
            gs.matchPhase = MatchPhase::Running;
        }
        gs.matchResult = MatchResult::None;
        gs.authoritativeMatchFinished = false;
        gs.isGameOver = false;
        gs.gameOverReason = ToGameOverReason(ComputeGameOverReasonForResult(gs.matchResult));
        return;
    }

    const MatchPhase previousPhase = gs.matchPhase;
    gs.matchPhase = MatchPhase::Finished;
    if ((hostFinishedBySuccess && clientFinishedBySuccess)
        || (hostFinishedByFailure && clientFinishedByFailure)) {
        gs.matchResult = MatchResult::Draw;
    } else if (hostFinishedBySuccess || clientFinishedByFailure) {
        gs.matchResult = MatchResult::LocalWin;
    } else {
        gs.matchResult = MatchResult::OpponentWin;
    }
    gs.authoritativeMatchFinished = true;
    gs.isGameOver = true;
    gs.gameOverReason = ToGameOverReason(ComputeGameOverReasonForResult(gs.matchResult));
    if (previousPhase != MatchPhase::Finished) {
        gs.matchJustFinished = true;
    }
}

/**
 * @brief 将服务端视角的比赛结果转换为客户端本地视角。
 * @param authoritativeResult 服务端权威结果
 * @return 客户端本地视角结果
 */
MatchResult Sys_Network::ToClientPerspective(MatchResult authoritativeResult) {
    switch (authoritativeResult) {
        case MatchResult::LocalWin:    return MatchResult::OpponentWin;
        case MatchResult::OpponentWin: return MatchResult::LocalWin;
        default:                       return authoritativeResult;
    }
}

/**
 * @brief 将多人比赛结果映射到现有 `gameOverReason` 编码。
 * @param result 比赛结果
 * @return 对应的 GameOver 原因码
 */
uint8_t Sys_Network::ComputeGameOverReasonForResult(MatchResult result) {
    switch (result) {
        case MatchResult::LocalWin:
            return 3;
        case MatchResult::OpponentWin:
            return 2;
        case MatchResult::Draw:
        case MatchResult::Disconnected:
        case MatchResult::None:
        default:
            return 0;
    }
}

/**
 * @brief 获取当前本地终局状态。
 * @details 直接返回 Res_GameState 中缓存的 localTerminalState，用于向服务端上报本地终局状态。
 * @param gs 当前比赛状态资源
 * @return 本地终局状态
 */
MultiplayerTerminalState Sys_Network::GetLocalTerminalState(const Res_GameState& gs) {
    return gs.localTerminalState;
}

/**
 * @brief 获取当前本地终局原因编码。
 * @details 直接返回 Res_GameState 中缓存的 localTerminalReason，用于向服务端上报本地终局原因。
 * @param gs 当前比赛状态资源
 * @return 本地终局原因编码（与 gameOverReason 对应）
 */
uint8_t Sys_Network::GetLocalTerminalReason(const Res_GameState& gs) {
    return gs.localTerminalReason;
}

bool Sys_Network::IsFinalTerminalState(MultiplayerTerminalState state) {
    return state == MultiplayerTerminalState::Death
        || state == MultiplayerTerminalState::Timeout
        || state == MultiplayerTerminalState::FinishedVictory
        || state == MultiplayerTerminalState::FinishedScoreFail;
}

/**
 * @brief 将阶段进度限制在 `0..kMultiplayerStageCount` 范围内。
 * @param progress 原始阶段值
 * @return 裁剪后的阶段值
 */
uint8_t Sys_Network::ClampStageProgress(uint8_t progress) {
    return std::min<uint8_t>(progress, kMultiplayerStageCount);
}

/**
 * @brief 根据双方最远推进的阶段数推导当前轮次索引。
 * @param hostStageProgress Host 已完成阶段数
 * @param clientStageProgress Client 已完成阶段数
 * @return 当前轮次索引
 */
uint8_t Sys_Network::ComputeCurrentRoundIndex(uint8_t hostStageProgress, uint8_t clientStageProgress) {
    const uint8_t furthestStage = std::max(ClampStageProgress(hostStageProgress), ClampStageProgress(clientStageProgress));
    if (furthestStage >= kMultiplayerStageCount) {
        return kMultiplayerStageCount - 1;
    }
    return furthestStage;
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
        
        // 目前测试场景无需真实速度，但必须初始化为 0，防止发送未初始化的内存垃圾数据
        pkt.linearVel[0] = pkt.linearVel[1] = pkt.linearVel[2] = 0.0f;
        
        SendPacket(resNet, pkt, NetTarget::Broadcast, NetDelivery::Unreliable);
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
    }
    m_Registry = nullptr;

    if (!reg.has_ctx<Res_Network>()) {
        LOG_WARN("[Sys_Network] Res_Network missing during OnDestroy; skipping ENet teardown to avoid orphaning a live host.");
        return;
    }

    auto& resNet = reg.ctx<Res_Network>();

    if (resNet.preserveSessionOnSceneExit && resNet.mode != PeerType::OFFLINE) {
        resNet.preserveSessionOnSceneExit = false;
        ResetSceneLocalState(resNet);
        LOG_INFO("[Sys_Network] Preserving ENet session across scene transition.");
        return;
    }

    if (resNet.host) {
        enet_host_destroy(resNet.host);
    }
    enet_deinitialize();
    ResetNetworkRuntimeState(resNet, false);
    LOG_INFO("Network System shut down. Sent: " << resNet.packetsSent << ", Received: " << resNet.packetsReceived);
}

/**
 * @brief 判断当前 ENet 会话是否仍然健康且可被新场景复用。
 * @param resNet 网络资源对象
 * @return `true` 表示会话仍可复用
 */
bool Sys_Network::CanReuseSession(const Res_Network& resNet) {
    if (resNet.host == nullptr || resNet.mode == PeerType::OFFLINE) {
        return false;
    }

    if (resNet.mode == PeerType::CLIENT) {
        return resNet.connected
            && resNet.peer != nullptr
            && resNet.peer->state == ENET_PEER_STATE_CONNECTED;
    }

    return true;
}

/**
 * @brief 清理与当前场景实体绑定相关的网络状态。
 * @param resNet 网络资源对象
 */
void Sys_Network::ResetSceneLocalState(Res_Network& resNet) {
    resNet.netIdMap.clear();
    resNet.nextNetID = 1u;
    resNet.localPlayerEntity = Entity::NULL_ENTITY;
    resNet.remoteGhostEntity = Entity::NULL_ENTITY;
}

/**
 * @brief 将 `Res_Network` 重置到不持有活动会话的干净运行态。
 * @param resNet 网络资源对象
 * @param keepConfiguration 是否保留 mode/IP/port 配置以便随后重新初始化
 */
void Sys_Network::ResetNetworkRuntimeState(Res_Network& resNet, bool keepConfiguration) {
    const PeerType previousMode = resNet.mode;
    const MultiplayerMode previousMultiplayerMode = resNet.multiplayerMode;
    char previousIP[sizeof(resNet.serverIP)] = {};
    strncpy_s(previousIP, sizeof(previousIP), resNet.serverIP, _TRUNCATE);
    const uint16_t previousPort = resNet.serverPort;

    resNet.host = nullptr;
    resNet.peer = nullptr;
    resNet.localClientID = (keepConfiguration && previousMode == PeerType::SERVER) ? 0u : UINT32_MAX;
    resNet.rtt = 0u;
    resNet.connected = false;
    resNet.remotePeerConnected = false;
    resNet.matchSetupReceived = false;
    resNet.bootstrapSceneActive = false;
    resNet.preserveSessionOnSceneExit = false;
    resNet.nextNetID = 1u;
    resNet.netIdMap.clear();
    resNet.localPlayerEntity = Entity::NULL_ENTITY;
    resNet.remoteGhostEntity = Entity::NULL_ENTITY;
    resNet.remoteGhostSnapshotValid = false;
    resNet.remoteGhostSnapshotRoundIndex = 0u;
    resNet.remoteGhostSnapshotPos[0] = 0.0f;
    resNet.remoteGhostSnapshotPos[1] = 0.0f;
    resNet.remoteGhostSnapshotPos[2] = 0.0f;
    resNet.remoteGhostSnapshotRot[0] = 0.0f;
    resNet.remoteGhostSnapshotRot[1] = 0.0f;
    resNet.remoteGhostSnapshotRot[2] = 0.0f;
    resNet.remoteGhostSnapshotRot[3] = 1.0f;
    resNet.packetsSent = 0u;
    resNet.packetsReceived = 0u;
    resNet.bytesSent = 0u;
    resNet.bytesReceived = 0u;

    if (keepConfiguration) {
        resNet.mode = previousMode;
        resNet.multiplayerMode = previousMultiplayerMode;
        strncpy_s(resNet.serverIP, sizeof(resNet.serverIP), previousIP, _TRUNCATE);
        resNet.serverPort = previousPort;
    } else {
        resNet.mode = PeerType::OFFLINE;
        resNet.multiplayerMode = MultiplayerMode::SameMapGhostRace;
        resNet.serverIP[0] = '\0';
        resNet.serverPort = 0u;
    }
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
    SendPacket(resNet, pkt, isServer ? NetTarget::Broadcast : NetTarget::Single, NetDelivery::Reliable);
}

} // namespace ECS
