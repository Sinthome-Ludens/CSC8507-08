#pragma once

#include <cstdint>

namespace ECS {

/// Registry context resource — 多人对战大厅状态（纯 POD）
struct Res_LobbyState {
    char     joinIP[16]     = "127.0.0.1";   // 加入游戏时的目标 IP
    uint16_t port           = 32499;          // 端口号
    bool     ipInputActive  = false;          // IP 输入框焦点状态（由 ImGui 更新）
};

} // namespace ECS
