#pragma once

#include <cstdint>

namespace ECS {

/// Registry context resource — 多人对战大厅状态（纯 POD）
struct Res_LobbyState {
    char     joinIP[16]     = "127.0.0.1";   // 加入游戏时的目标 IP
    uint16_t port           = 32499;          // 端口号
    int8_t   ipCursorPos    = 9;              // IP 输入光标位置（字符数）
    bool     ipInputActive  = false;          // IP 输入激活状态
};

} // namespace ECS
