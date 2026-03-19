/**
 * @file Sys_Network.h
 * @brief ENet 网络系统声明。
 *
 * @details
 * 定义 `ECS::Sys_Network` 的生命周期、数据包分发器，以及网络事件订阅接口。
 */
#pragma once

#include "Core/ECS/SystemManager.h"
#include "Core/ECS/EventBus.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
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

    /**
     * @brief 注册数据包类型到成员处理函数的分发表。
     */
    void RegisterHandlers();

    // ── 初始化阶段函数 ──
    /**
     * @brief 订阅网络系统依赖的本地事件总线回调。
     * @param reg ECS 注册表
     */
    void InitializeEvents(Registry& reg);
    /**
     * @brief 初始化服务端 ENet host，并准备分配客户端 ID。
     * @param resNet 网络资源对象
     */
    void InitializeServer(Res_Network& resNet);
    /**
     * @brief 初始化客户端 ENet host，并向目标地址发起连接。
     * @param resNet 网络资源对象
     */
    void InitializeClient(Res_Network& resNet);

    // ── 内部阶段函数 ──
    /**
     * @brief 拉取并分发所有待处理的底层网络事件。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void ProcessNetworkEvents(Registry& reg, Res_Network& resNet);
    /**
     * @brief 根据包头类型将收到的数据包派发给对应处理函数。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleReceivePacket(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理本地输入的采集、发送与主机本地驱动。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void HandleLocalInput(Registry& reg, Res_Network& resNet);
    /**
     * @brief 同步本地玩家位姿到远端幽灵显示通道。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void SyncGhostTransforms(Registry& reg, Res_Network& resNet);
    /**
     * @brief 将服务端权威世界状态广播给客户端。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void BroadcastWorldState(Registry& reg, Res_Network& resNet);

    // ── 数据包处理回调函数 ──
    /**
     * @brief 处理服务端分配客户端身份的欢迎包。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleWelcomePacket(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理服务端广播的 Transform 同步包。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleSyncTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理服务端广播的比赛状态快照。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleMatchState(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理服务端广播的多人重开指令。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleMatchRestart(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理服务端下发的同图模式配置与权威地图序列。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleMultiplayerSetup(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理客户端上报的输入位掩码。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleClientInput(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理客户端上报的幽灵位姿。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleClientGhostTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理客户端上报的三阶段比赛进度与终局状态。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleClientMatchProgress(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理客户端发起的多人重开请求。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleClientMatchRestartRequest(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理网络同步过来的离散玩法事件。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleGameAction(Registry& reg, Res_Network& resNet, const ENetEvent& event);
    /**
     * @brief 处理服务端广播的远端幽灵位姿。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param event 底层 ENet 接收事件
     */
    void HandleSyncGhostTransform(Registry& reg, Res_Network& resNet, const ENetEvent& event);

    /**
     * @brief 清空仅在单帧内有效的比赛状态边沿标记。
     * @param reg ECS 注册表
     */
    void ResetFrameFlags(Registry& reg);
    /**
     * @brief 由客户端上报本地阶段进度与待确认终局状态。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void UpdateClientMatchProgress(Registry& reg, Res_Network& resNet);
    /**
     * @brief 处理本地 UI 触发的多人重开请求。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void ProcessMatchRestartRequest(Registry& reg, Res_Network& resNet);
    /**
     * @brief 当服务端权威比赛状态发生变化时广播新的快照。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param force 是否忽略脏检查强制广播
     */
    void BroadcastMatchStateIfDirty(Registry& reg, Res_Network& resNet, bool force = false);
    /**
     * @brief 由服务端向双方广播多人重开指令。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     */
    void BroadcastMatchRestart(Registry& reg, Res_Network& resNet);
    /**
     * @brief 由服务端向目标对端广播同图模式配置与权威地图序列。
     * @param reg ECS 注册表
     * @param resNet 网络资源对象
     * @param explicitPeer 目标对端；为空时广播给所有客户端
     */
    void BroadcastMultiplayerSetup(Registry& reg, Res_Network& resNet, ENetPeer* explicitPeer = nullptr);
    /**
     * @brief 将当前多人战局状态重置为新一局的初始值。
     * @param reg ECS 注册表
     */
    void ResetMatchStateForRestart(Registry& reg);
    /**
     * @brief 根据本地/远端阶段与终局终态收口权威比赛结果。
     * @param gs 当前比赛状态资源（包含 localTerminalState / remoteTerminalState）
     */
    void ApplyMatchResult(Res_GameState& gs);
    /**
     * @brief 将 Finished 比赛状态映射到 GameOver UI。
     * @param reg ECS 注册表
     */
    void UpdateMatchUIState(Registry& reg);
    /**
     * @brief 将服务端视角的胜负结果转换为客户端本地视角。
     * @param authoritativeResult 服务端权威结果
     * @return 客户端本地视角下的结果
     */
    static MatchResult ToClientPerspective(MatchResult authoritativeResult);
    /**
     * @brief 将多人比赛结果映射到现有单机 GameOver reason 编码。
     * @param result 比赛结果
     * @return 对应的 gameOverReason
     */
    static uint8_t ComputeGameOverReasonForResult(MatchResult result);
    /**
     * @brief 从本地玩法/界面状态统一推导客户端当前待上报的终局原因。
     * @param reg ECS 注册表
     * @param gs 当前比赛状态资源
     * @return 0 表示尚未进入本地终局，否则返回现有 gameOverReason 编码
     */
    static MultiplayerTerminalState GetLocalTerminalState(const Res_GameState& gs);
    static uint8_t GetLocalTerminalReason(const Res_GameState& gs);
    static bool IsFinalTerminalState(MultiplayerTerminalState state);
    /**
     * @brief 判断当前 `Res_Network` 是否仍持有可安全跨场景复用的 ENet 会话。
     * @param resNet 网络资源对象
     * @return `true` 表示 host/peer 仍然有效，可跳过重新建连
     */
    static bool CanReuseSession(const Res_Network& resNet);
    /**
     * @brief 清理仅与当前场景实体绑定相关的网络状态。
     * @details 用于跨场景保留连接时移除旧实体映射，避免把上一张图的 EntityID 带到新场景。
     * @param resNet 网络资源对象
     */
    static void ResetSceneLocalState(Res_Network& resNet);
    /**
     * @brief 将 `Res_Network` 重置为不持有活动会话的干净运行态。
     * @details 可选择保留模式/IP/端口配置，供随后重新初始化联机。
     * @param resNet 网络资源对象
     * @param keepConfiguration 是否保留 mode/serverIP/serverPort
     */
    static void ResetNetworkRuntimeState(Res_Network& resNet, bool keepConfiguration);
    /**
     * @brief 将阶段进度限制在合法的 `0..kMultiplayerStageCount` 范围内。
     * @param progress 原始阶段值
     * @return 裁剪后的阶段值
     */
    static uint8_t ClampStageProgress(uint8_t progress);
    /**
     * @brief 根据双方最远推进阶段推导当前轮次索引。
     * @param hostStageProgress Host 阶段数
     * @param clientStageProgress Client 阶段数
     * @return 当前应显示的轮次索引
     */
    static uint8_t ComputeCurrentRoundIndex(uint8_t hostStageProgress, uint8_t clientStageProgress);
    /**
     * @brief 将服务端权威地图序列写入会话级 UI 状态。
     * @param reg ECS 注册表
     * @param mapSequence 三关地图序列
     * @param roundIndex 当前轮次索引
     */
    static void ApplyAuthoritativeMapSequence(Registry& reg,
                                              const uint8_t* mapSequence,
                                              uint8_t roundIndex);
    static EntityID EnsureRemoteGhostEntity(Registry& reg, Res_Network& resNet);
    static void CacheRemoteGhostSnapshot(Res_Network& resNet, const Net_Packet_GhostTransform& pkt);
    static void ApplyCachedRemoteGhostSnapshot(Registry& reg,
                                               Res_Network& resNet,
                                               bool resetInterpolationBuffer);
    static void RefreshRemoteGhostEntity(Registry& reg, Res_Network& resNet);
    static void HideRemoteGhostEntity(Registry& reg, Res_Network& resNet);

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
    void UpdatePlayerInput(Registry& reg, uint32_t clientID, uint32_t buttonMask);

    /**
     * @brief 统一发送辅助函数：封装数据包创建、发送及统计逻辑。
     * @tparam T 数据包结构体类型
     * @param resNet 网络资源引用
     * @param packet 要发送的数据包对象
     * @param target 发送范围（单播或广播）
     * @param delivery 传输可靠性
     * @param explicitPeer 如果 target 是 Single 且提供了此参数，则发送给该 peer。若为 nullptr 则使用 resNet.peer。
     */
    template<typename T>
    void SendPacket(Res_Network& resNet, T& packet, NetTarget target = NetTarget::Single, NetDelivery delivery = NetDelivery::Unreliable, ENetPeer* explicitPeer = nullptr) {
        // 将枚举转换为 ENet 内部标识
        enet_uint32 flags = 0;
        if (delivery == NetDelivery::Reliable) {
            flags |= ENET_PACKET_FLAG_RELIABLE;
        }

        ENetPacket* p = enet_packet_create(&packet, sizeof(T), flags);
        
        if (target == NetTarget::Broadcast) {
            enet_host_broadcast(resNet.host, 0, p);
        } else {
            ENetPeer* targetPeer = explicitPeer ? explicitPeer : resNet.peer;
            if (targetPeer) {
                enet_peer_send(targetPeer, 0, p);
            } else {
                enet_packet_destroy(p);
                return;
            }
        }
        resNet.packetsSent++;
        resNet.bytesSent += sizeof(T);
    }

    // ── 事件监听与发送 ──
    Registry* m_Registry = nullptr; ///< 缓存的注册表指针，用于在事件回调中获取网络资源
    SubscriptionID m_ActionSubID = 0; ///< 保存事件订阅的 ID
    uint32_t m_NextClientID = 1; ///< 服务端分配给下一个连接客户端的 ID
    uint32_t m_LastInputMask = 0; ///< 记录客户端上一帧的输入，用于判断状态变化
    float m_InputTimer = 0.0f;    ///< 客户端输入发送计时器
    float m_GhostTransformTimer = 0.0f; ///< 幽灵位姿同步计时器
    EntityID m_LastGhostSourceEntity = Entity::NULL_ENTITY;
    uint8_t m_LastGhostSentRoundIndex = 0xFF;
    uint8_t m_LastReportedLocalStageProgress = 0xFF;
    uint8_t m_LastReportedLocalGameOverReason = 0xFF;
    uint8_t m_LastBroadcastPhase = 0xFF;
    uint8_t m_LastBroadcastResult = 0xFF;
    uint8_t m_LastBroadcastHostStage = 0xFF;
    uint8_t m_LastBroadcastClientStage = 0xFF;
    uint8_t m_LastBroadcastRoundIndex = 0xFF;
    uint8_t m_LastBroadcastGameOverReason = 0xFF;

    /**
     * @brief 本地游戏动作事件回调。
     *
     * @details
     * 由本机发布的 `Evt_Net_GameAction` 事件触发，用于将本地输入/状态变化转换为网络数据包
     * 或本地网络状态更新。
     *
     * 线程与生命周期假设：
     * - 在主游戏线程中由 EventBus 调用，不在 ENet 内部工作线程中执行。
     * - 仅在 `Sys_Network::OnAwake` 成功完成且未调用 `OnDestroy` 期间有效，此时 `m_Registry` 保证非空。
     * - 不应发生重入调用，调用方需保证按帧序或事件队列顺序串行触发。
     *
     * @param evt 描述本地玩家动作或网络相关请求的事件载体。
     */
    void OnLocalGameAction(const Evt_Net_GameAction& evt);
};

} // namespace ECS
