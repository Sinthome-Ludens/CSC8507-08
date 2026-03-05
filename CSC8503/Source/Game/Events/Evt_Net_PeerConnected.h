#pragma once

#include <cstdint>

/**
 * @brief 网络节点连接事件（延迟分发）
 *
 * 由 Sys_Network 在新的客户端成功连接并完成握手（或者服务器建立连接）后发布。
 * 监听者：Sys_GameManager, Sys_UI 等。
 *
 * @note 使用延迟发布模式（bus.publish_deferred<Evt_Net_PeerConnected>），
 *       确保状态在所有逻辑处理前更新一致。
 */
struct Evt_Net_PeerConnected {
    uint32_t clientID; ///< 新连接的客户端 ID
};
