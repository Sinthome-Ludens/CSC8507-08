/**
 * @file C_D_NetworkIdentity.h
 * @brief 网络身份组件：为需要进行网络同步的实体提供唯一的身份标识
 *
/**
 * @details
 * 网络身份标识（Network Identity）
 * * 本结构体用于解决多人环境下的两个核心问题：
 * 1. 它是谁？ (通过全局唯一的 netID 区分实体)
 * 2. 谁说了算？ (通过 ownerClientID 判定修改权限和同步策略)
 *
 * ## 强制性规范
 * - 【唯一性】：netID 必须在全网范围内全局唯一。1 为起始值，0 严禁用于有效对象。
 * - 【所有权】：ownerClientID 为 0 表示该实体由 Server 托管；非 0 表示由对应 ID 的 Client 控制。
 * - 【操作权】：在处理数据更新包时，必须校验发包者的 ClientID 是否与此处的 owner 一致。
 * - 【性能限制】：本结构体为 8 字节 POD 类型，设计上禁止添加虚函数或复杂对象，以确保高效透传。
 */
#pragma once

#include <cstdint>

namespace ECS {

/**
 * @brief 网络身份组件
 *
 * @details
 * 挂载了此组件的实体将被加入网络同步系统。
 */
struct C_D_NetworkIdentity {
    uint32_t netID         = 0; ///< 全网唯一的实体标识符
    uint32_t ownerClientID = 0; ///< 实体的归属客户端 ID（由哪位玩家产生或控制）
};

static_assert(sizeof(C_D_NetworkIdentity) == 8, "C_D_NetworkIdentity must be 8 bytes POD.");

} // namespace ECS
