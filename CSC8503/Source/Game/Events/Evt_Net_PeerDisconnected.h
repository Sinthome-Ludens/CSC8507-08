#pragma once

#include <cstdint>

/**
 * @brief 网络节点断开连接事件（延迟分发）
 *
 * 由 Sys_Network 在检测到连接断开、超时或主动关闭时发布。
 * 监听者：Sys_GameManager, Sys_UI 等。
 *
 * @note 使用延迟发布模式（bus.publish_deferred<Evt_Net_PeerDisconnected>），
 *       确保断线后的清理工作（如销毁其对应实体）能被统一处理。
 */
struct Evt_Net_PeerDisconnected {
    uint32_t clientID; ///< 断开连接的客户端 ID
};
