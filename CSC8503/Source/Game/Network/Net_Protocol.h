/**
 * @file Net_Protocol.h
 * @brief 定义多人联机使用的固定线协议包类型与包体结构。
 *
 * @details
 * 所有 `Net_PacketType` 都显式绑定稳定的数值 ID，避免新增包类型时重排旧枚举值，
 * 从而破坏不同构建之间的基础线协议兼容性。若未来需要引入破坏性协议变更，
 * 应同步提升 `Net_Packet_Handshake::protocolVersion` 并在连接阶段做版本校验。
 */
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
    SYS_HANDSHAKE = 0,             ///< 客户端发送握手请求
    SYS_WELCOME = 1,               ///< 服务端确认连接并发送初始化数据
    SYS_DISCONNECT = 2,            ///< 断开连接通知
    SYNC_TRANSFORM = 3,            ///< Transform状态同步数据包
    SYNC_MATCH_STATE = 4,          ///< 比赛状态同步数据包
    SYNC_MATCH_RESTART = 5,        ///< 服务端广播多人重开指令
    GAME_EVENT = 6,                ///< 游戏事件数据包（客户端或服务端在发生攻击、交互、技能释放等游戏行为时发送）
    CLIENT_INPUT = 7,              ///< 客户端输入数据包（用于服务器权威架构）
    CLIENT_MATCH_PROGRESS = 8,     ///< 客户端上报当前比赛进度
    CLIENT_MATCH_RESTART_REQUEST = 9, ///< 客户端请求服务端执行多人重开
    SYNC_MULTIPLAYER_SETUP = 10    ///< 服务端下发同图联机模式与地图序列
};

// 传输可靠性
enum class NetDelivery {
    Unreliable = 0,               // 不可靠传输（UDP特性，快，可能丢包）
    Reliable   = 1                // 可靠传输（类似TCP重传，慢，保证到达）
};

// 发送范围
enum class NetTarget {
    Single,    // 发送给当前绑定的 peer
    Broadcast  // 广播给所有连接的客户端
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
 * @struct Net_Packet_MatchState
 * @brief 服务端广播的多人比赛状态。
 *
 * 只同步单本地玩家多人模式需要的状态，不依赖远端玩家实体。
 */
struct Net_Packet_MatchState : public Net_PacketHeader {
    uint8_t matchPhase;
    uint8_t matchResult;
    uint8_t hostStageProgress;
    uint8_t clientStageProgress;
    uint8_t currentRoundIndex;
    uint8_t gameOverReason;
};

struct Net_Packet_MatchRestart : public Net_PacketHeader {
    uint8_t multiplayerMode = 0;
    uint8_t mapSequence[3] = {};
    uint8_t currentRoundIndex = 0;
};

struct Net_Packet_MultiplayerSetup : public Net_PacketHeader {
    uint8_t multiplayerMode = 0;
    uint8_t mapSequence[3] = {};
    uint8_t currentRoundIndex = 0;
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
    uint32_t buttonMask;
};

/**
 * @struct Net_Packet_ClientMatchProgress
 * @brief 客户端上报自己的三阶段比赛进度。
 */
struct Net_Packet_ClientMatchProgress : public Net_PacketHeader {
    uint8_t stageProgress;
    uint8_t currentRoundIndex;
    uint8_t reportedFinished;
    uint8_t gameOverReason;
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
static_assert(sizeof(Net_Packet_MatchState) == 11, "Net_Packet_MatchState size mismatch");
static_assert(sizeof(Net_Packet_MatchRestart) == 10, "Net_Packet_MatchRestart size mismatch");
static_assert(sizeof(Net_Packet_MultiplayerSetup) == 10, "Net_Packet_MultiplayerSetup size mismatch");
static_assert(sizeof(Net_Packet_GameAction) == 18, "Net_Packet_GameAction size mismatch");
static_assert(sizeof(Net_Packet_ClientInput) == 9, "Net_Packet_ClientInput size mismatch");
static_assert(sizeof(Net_Packet_ClientMatchProgress) == 9, "Net_Packet_ClientMatchProgress size mismatch");

}
