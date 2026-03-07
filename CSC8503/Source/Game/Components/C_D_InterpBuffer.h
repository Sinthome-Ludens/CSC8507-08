/**
 * @file C_D_InterpBuffer.h
 * @brief 插值缓冲组件：用于对远程实体的状态进行平滑插值，消除网络抖动。
 *
 * @details
 * 远程实体插值缓冲区（Remote Interpolation Buffer）
 * * 本组件专门用于平滑处理网络波动，消除远程对象在本地表现出的“跳变”或“平移感”。
 *
 * ## 挂载策略
 * - 【按需挂载】：仅挂载于“非本地控制”的对象上。
 * - 【本地零延迟】：本地操控的实体（Local Owner）严禁挂载本组件，以确保输入响应达到 0 延迟。
 *
 * ## 平滑机制（插值原理）
 * 1. 【缓冲队列】：内置 10 帧容量的环形快照数组。新到的网络包会被存入队列，等待处理。
 * 2. 【平滑渲染】：由 `Sys_Interpolation` 驱动，在最近的两个历史快照之间进行线性插值（Lerp/Slerp），从而还原出平滑的运动轨迹。
 *
 * ## 内存负载说明
 * - 内存开销：单实体固定分配 320 bytes（10 * 32 bytes 快照）。
 * - 性能影响：由于仅对远程实体生效，其总内存压力正比于当前场景中的在线玩家数量。
 */
#pragma once

#include "Vector.h"
#include "Quaternion.h"

namespace ECS {

/**
 * @brief 记录某一时刻对象姿态的快照。
 */
struct TransformSnapshot {
    NCL::Maths::Vector3    pos{0.0f, 0.0f, 0.0f};           ///< 位置快照（米）
    NCL::Maths::Quaternion rot{0.0f, 0.0f, 0.0f, 1.0f};     ///< 旋转快照
    float                  timestamp = 0.0f;                ///< 数据包关联的时刻戳（毫秒）
};

/**
 * @brief 插值缓冲组件
 *
 * @details
 * 仅由 Sys_Network 在接收到别人数据时写入最新值，由 Sys_Interpolation 读取历史快照做平滑。
 */
struct C_D_InterpBuffer {
    static constexpr int CAPACITY = 10;
    
    TransformSnapshot snapshots[CAPACITY]; ///< 环形快照缓冲数组
    int               head  = 0;           ///< 指向最新插入的一项索引（写指针）
    int               count = 0;           ///< 当前缓冲区内的有效元素个数

};

/**
 * @brief 向插值缓冲区添加一个状态快照（自由函数，保持组件为纯数据）
 *
 * 丢弃时间戳 <= 最新快照的乱序包，保证环形缓冲区严格按时间递增排列，
 * 避免 Sys_Interpolation 线性索引时因乱序导致插值崩溃或画面闪烁。
 */
inline void InterpBuffer_AddSnapshot(C_D_InterpBuffer& buf,
    const NCL::Maths::Vector3& pos, const NCL::Maths::Quaternion& rot, float timestamp) {
    // 丢弃乱序/重复包：新时间戳必须严格大于缓冲区中最新快照的时间戳
    if (buf.count > 0) {
        int latest = (buf.head - 1 + C_D_InterpBuffer::CAPACITY) % C_D_InterpBuffer::CAPACITY;
        if (timestamp <= buf.snapshots[latest].timestamp) {
            return; // 过时包，直接丢弃
        }
    }

    buf.snapshots[buf.head].pos = pos;
    buf.snapshots[buf.head].rot = rot;
    buf.snapshots[buf.head].timestamp = timestamp;

    buf.head = (buf.head + 1) % C_D_InterpBuffer::CAPACITY;
    if (buf.count < C_D_InterpBuffer::CAPACITY) {
        buf.count++;
    }
}

} // namespace ECS
