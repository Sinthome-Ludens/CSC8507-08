#pragma once

#include <cstdint>

namespace NCL::CSC8503::ECS {

#pragma pack(push, 1)

    enum PacketType : uint8_t {
        SYS_HANDSHAKE = 0,
        SYS_WELCOME,
        SYS_DISCONNECT,
        SYNC_TRANSFORM,
        GAME_EVENT
    };

    struct NetPacketHeader {
        PacketType type;
        uint32_t timestamp;
    };

    struct Packet_Handshake : public NetPacketHeader {
        uint32_t protocolVersion;
    };

    struct Packet_Welcome : public NetPacketHeader {
        uint32_t clientID;
        uint32_t levelID;
        float spawnX;
        float spawnY;
        float spawnZ;
    };

    struct Packet_Transform : public NetPacketHeader {
        uint32_t netID;
        float posX;
        float posY;
        float posZ;
        float rotX;
        float rotY;
        float rotZ;
        float rotW;
        float velX;
        float velY;
        float velZ;
    };

    struct Packet_GameAction : public NetPacketHeader {
        uint32_t sourceNetID;
        uint32_t targetNetID;
        uint8_t actionCode;
        int32_t param1;
    };

#pragma pack(pop)

    // Ensure all sizes meet expectations to avoid cross-platform alignment issues
    static_assert(sizeof(NetPacketHeader) == 5, "NetPacketHeader size mismatch");
    static_assert(sizeof(Packet_Handshake) == 9, "Packet_Handshake size mismatch");
    static_assert(sizeof(Packet_Welcome) == 25, "Packet_Welcome size mismatch");
    static_assert(sizeof(Packet_Transform) == 49, "Packet_Transform size mismatch");
    static_assert(sizeof(Packet_GameAction) == 18, "Packet_GameAction size mismatch");

} // namespace NCL::CSC8503::ECS