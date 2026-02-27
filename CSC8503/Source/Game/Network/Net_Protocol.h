#pragma once
#include <cstdint>

namespace ECS {

#pragma pack(push, 1)

/**
 * @enum Net_PacketType
 * @brief 网络数据包类型枚举
 *
 * 用于标识当前数据包的具体类型。
 * 前缀用于区分数据包所属模块：
 *
 * - SYS  ：系统级数据包（连接管理）
 * - SYNC ：状态同步数据包
 * - GAME ：游戏业务逻辑数据包
 */
enum Net_PacketType : uint8_t {
    SYS_HANDSHAKE = 0,   ///< 客户端发送握手请求
    SYS_WELCOME,         ///< 服务端确认连接并发送初始化数据
    SYS_DISCONNECT,      ///< 断开连接通知
    SYNC_TRANSFORM,      ///< Transform状态同步数据包
    GAME_EVENT,          ///< 游戏事件数据包（客户端或服务端在发生攻击、交互、技能释放等游戏行为时发送）
    CLIENT_INPUT         ///< 客户端输入数据包（用于服务器权威架构）
};

/**
 * @struct Net_PacketHeader
 * @brief 所有网络数据包的公共包头
 *
 * 每个数据包必须包含该结构，用于标识数据包类型和时间信息，
 * 便于接收端正确解析并进行时序同步。
 */
struct Net_PacketHeader {
    Net_PacketType type; ///< 数据包类型标识
    uint32_t timestamp;  ///< 数据包发送时间戳（毫秒），用于延迟计算与同步
};

/**
 * @struct Net_Packet_Handshake
 * @brief 客户端握手数据包
 *
 * 客户端在建立连接时发送，用于通知服务端客户端的协议版本，
 * 服务端可据此判断是否兼容。
 */
struct Net_Packet_Handshake : public Net_PacketHeader {
    uint32_t protocolVersion; ///< 客户端使用的协议版本号
};

/**
 * @struct Net_Packet_Welcome
 * @brief 服务端欢迎数据包
 *
 * 服务端在接受客户端连接后发送，用于初始化客户端网络身份
 * 和玩家出生位置。
 */
struct Net_Packet_Welcome : public Net_PacketHeader {
    uint32_t levelID  = 1;       ///< 默认从第 1 关开始
    float spawnX = 0.0f;         ///< 坐标默认 0
    float spawnY = 0.0f;
    float spawnZ = 0.0f;
    uint32_t clientID;       ///< 默认 ID 为 0
};

/**
 * @struct Net_Packet_Transform
 * @brief Transform同步数据包
 *
 * 用于同步实体的位置、旋转和速度信息。
 * 通常由服务端高频发送，用于保持客户端状态一致。
 */
struct Net_Packet_Transform : public Net_PacketHeader {
    uint32_t netID; ///< 实体网络唯一ID
    float pos[3];            ///< 位置 (x, y, z)
    float rot[4];            ///< 旋转四元数 (x, y, z, w)
    float linearVel[3];      ///< 线性速度（可选，用于预测）
};

/**
 * @struct Net_Packet_GameAction
 * @brief 游戏行为数据包
 *
 * 用于传输游戏中的瞬时行为，例如：
 * - 攻击
 * - 技能释放
 * - 物品交互
 */
struct Net_Packet_GameAction : public Net_PacketHeader {
    uint32_t sourceNetID; ///< 行为发起实体ID
    uint32_t targetNetID; ///< 目标实体ID（无目标时为0）

    uint8_t actionCode;   ///< 行为类型编码（业务层定义）

    int32_t param1;       ///< 自定义参数（例如伤害值或物品ID）
};

/**
 * @struct Net_Packet_ClientInput
 * @brief 客户端输入数据包
 *
 * 客户端将键盘/手柄的意图打包发送给服务端，由服务端进行物理模拟。
 */
struct Net_Packet_ClientInput : public Net_PacketHeader {
    bool up;
    bool down;
    bool left;
    bool right;
};

#pragma pack(pop)

/**
 * @brief 编译期结构体大小检查
 *
 * 确保 struct 使用 1 字节对齐，
 * 避免因 padding 导致网络传输解析错误。
 */
static_assert(sizeof(Net_PacketHeader) == 5, "Net_PacketHeader size mismatch");
static_assert(sizeof(Net_Packet_Handshake) == 9, "Net_Packet_Handshake size mismatch");
static_assert(sizeof(Net_Packet_Welcome) == 25, "Net_Packet_Welcome size mismatch");
static_assert(sizeof(Net_Packet_Transform) == 49, "Net_Packet_Transform size mismatch");
static_assert(sizeof(Net_Packet_GameAction) == 18, "Net_Packet_GameAction size mismatch");
static_assert(sizeof(Net_Packet_ClientInput) == 9, "Net_Packet_ClientInput size mismatch");

} // namespace NCL::CSC8503::ECS
