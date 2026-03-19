/**
 * @file Res_Network.h
 * @brief 全局网络资源：存储当前实例的网络状态、连接信息与统计数据
 *
 * @details
 * `Res_Network` 是 Sys_Network 的核心数据结构，存储本机的角色（Server/Client）、
 * 底层的 ENet 句柄、网络映射表以及流量统计信息，供调试面板（Sys_ImGui）和其他系统读取。
 */

#pragma once

#include <cstdint>
#include <unordered_map>
#include "Core/ECS/EntityID.h"

// 前向声明 ENet 结构体，避免暴露 enet.h 给包含了此头文件的文件
struct _ENetHost;
struct _ENetPeer;

namespace ECS {

/**
 * @brief 网络连接模式枚举
 */
enum class PeerType {
    OFFLINE, ///< 单机离线模式
    SERVER,  ///< 充当服务器主机
    CLIENT   ///< 充当客户端连接到主机
};

/**
 * @brief 多人比赛模式枚举。
 */
enum class MultiplayerMode : uint8_t {
    DifferentMapRace = 0,   ///< 双方各自独立随机地图竞速
    SameMapGhostRace = 1,   ///< 服务端统一选图，同图竞速并显示对手幽灵
};

/**
 * @brief 全局网络状态资源
 *
 * @details
 * 由 Sys_Network 在 OnAwake/OnUpdate 中维护，其余系统只读访问。
 */
struct Res_Network {
    PeerType mode = PeerType::OFFLINE; ///< 当前网络模式（服务器/客户端/单机）
    MultiplayerMode multiplayerMode = MultiplayerMode::SameMapGhostRace; ///< 当前多人比赛模式

    char     serverIP[16]   = "127.0.0.1"; ///< 目标服务器 IP（Server 模式下仅用于日志）
    uint16_t serverPort     = 32499;       ///< 监听 / 连接端口

    _ENetHost* host = nullptr;         ///< ENet host 指针（本地端点）
    _ENetPeer* peer = nullptr;         ///< 客户端连接到服务器时的唯一 peer 指针

    uint32_t localClientID = 0;        ///< 本机的分配 ID（Server 默认为 0，Client 由 Server 分配）
    uint32_t rtt           = 0;        ///< 当前延迟估算（Ping 值，单位 ms）
    bool     connected     = false;    ///< 是否已成功建立连接（Client 用于握手判定）
    bool     matchSetupReceived = false; ///< 是否已经收到服务端权威的同图比赛配置
    bool     bootstrapSceneActive = false; ///< 当前是否处于同图模式的联机引导场景
    bool     preserveSessionOnSceneExit = false; ///< 是否在当前场景退出时保留 ENet 会话以跨场景复用

    uint32_t nextNetID     = 1;        ///< 下一个可分配的网络实体 ID（仅 Server 维护）

    ///< @brief 网络 ID 到本地实体 ID 的映射表
    std::unordered_map<uint32_t, ECS::EntityID> netIdMap;

    // 网络流量统计（供 ImGui 调试面板读取）
    uint64_t packetsSent     = 0;      ///< 累计发送包数
    uint64_t packetsReceived = 0;      ///< 累计接收包数
    uint64_t bytesSent       = 0;      ///< 累计发送字节数
    uint64_t bytesReceived   = 0;      ///< 累计接收字节数
};

} // namespace ECS
