/**
 * @file Res_CampaignState.h
 * @brief 战役状态资源：地图定义表 + 运行时状态（session 级 ctx，跨场景存活）。
 */
#pragma once
#include <cstdint>

namespace ECS {

/// 战役地图定义（数据驱动）
struct CampaignMapDef {
    const char* meshFile;      ///< "Dock.obj"
    const char* navMeshFile;   ///< "Dock.navmesh"
    const char* pointsFile;    ///< "Dock.points"（可能不存在）
    const char* displayName;   ///< "DOCK"
};

inline constexpr CampaignMapDef kCampaignMaps[] = {
    { "Dock.obj",    "Dock.navmesh",    "Dock.points",    "DOCK"     },
    { "HangerA.obj", "HangerA.navmesh", "HangerA.points", "HANGER A" },
    { "HangerB.obj", "HangerB.navmesh", "HangerB.points", "HANGER B" },
    { "Helipad.obj", "Helipad.navmesh", "Helipad.points", "HELIPAD"  },
    { "Lab.obj",     "Lab.navmesh",     "Lab.points",     "LAB"      },
};
inline constexpr int kCampaignMapCount = 5;
inline constexpr int kCampaignRounds   = 3;

/// 战役运行状态（session 级 ctx，跨场景存活，不在 OnExit 中清除）
struct Res_CampaignState {
    int8_t  mapSequence[kCampaignRounds] = {-1, -1, -1}; ///< 选中的 3 张地图索引 (0-4)
    int8_t  currentRound = 0;                              ///< 当前轮次 (0, 1, 2)
    bool    active       = false;                          ///< 是否处于战役模式
    float   totalPlayTime = 0.0f;                          ///< 累计游玩时间（3 张地图合计）
};

} // namespace ECS
