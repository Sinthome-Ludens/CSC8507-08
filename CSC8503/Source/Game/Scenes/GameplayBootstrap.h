/**
 * @file GameplayBootstrap.h
 * @brief Gameplay 场景统一引导——消除 6 个 gameplay scene 的复制粘贴模板。
 *
 * 每个 gameplay scene 的 OnEnter/OnExit 只需传入 GameplaySceneConfig 并调用
 * 对应的 Bootstrap 函数，场景特定逻辑（地图名、multiplayer、forcedTreeId）
 * 通过 config 参数表达。
 */
#pragma once

#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Utils/MapLoader.h"
#include <memory>

class IScene;
namespace ECS { class NavMeshPathfinderUtil; }

struct Res_NCL_Pointers;

namespace ECS {

/**
 * @brief Gameplay 场景配置——每个 scene 的差异点全部收敛到这里。
 */
struct GameplaySceneConfig {
    const char* sceneName       = "Unknown";      ///< 场景名（日志用）
    const char* mapConfigJson   = nullptr;         ///< 地图 Prefab JSON 文件名
    bool        isMultiplayer   = false;           ///< 是否启用多人系统
    const char* forcedTreeId    = nullptr;         ///< 强制对话树 ID（nullptr = 不强制）
    bool        callAutoFillHUD = false;           ///< 是否调用 AutoFillHUDSlots（Tutorial 专用）
};

/**
 * @brief 统一 ctx 注册（OnEnter 前半段）。
 *
 * 注册所有 gameplay scene 共用的 ctx 资源。
 * 不包含 Res_GameState（由场景自行处理 multiplayer 保留逻辑）和
 * Res_ChatState（跨场景保留，由 forcedTreeId 参数控制是否强制设置 treeId）。
 */
void BootstrapEmplaceCtx(Registry& registry, ::IScene* scene, MeshHandle cubeMesh,
                          const GameplaySceneConfig& config);

/**
 * @brief 统一系统注册。
 *
 * 注册所有 gameplay scene 共用的系统（含 ImGui debug 系统）。
 * multiplayer 系统（Sys_Network/Sys_Interpolation）由 config.isMultiplayer 控制。
 */
void BootstrapRegisterSystems(SystemManager& systems, const GameplaySceneConfig& config);

/**
 * @brief 加载地图 + 创建 Orb + NavMesh 设置 + Minimap 缓存。
 *
 * @param outPathfinder 由 scene 持有的 pathfinder unique_ptr 引用
 * @return MapLoadResult 用于后续 multiplayer 设置
 */
MapLoadResult BootstrapLoadMap(Registry& registry, MeshHandle cubeMesh,
                                const GameplaySceneConfig& config,
                                std::unique_ptr<NavMeshPathfinderUtil>& outPathfinder);

/**
 * @brief AwakeAll 之后的统一设置（Audio/UI/Toast/Save/Equipment）。
 *
 * 包含 Res_GameState 的初始化/保留逻辑（根据 isMultiplayer 分支）。
 */
void BootstrapPostAwake(Registry& registry, const GameplaySceneConfig& config);

/**
 * @brief 统一 ctx 清理（OnExit）。
 *
 * 清理所有 gameplay scene 共用的 ctx 资源。
 * Res_GameState 在 singleplayer 时清理，multiplayer 时保留。
 * Res_ChatState 始终保留（只在 MainMenu 清理）。
 */
void BootstrapEraseCtx(Registry& registry, const GameplaySceneConfig& config);

} // namespace ECS
