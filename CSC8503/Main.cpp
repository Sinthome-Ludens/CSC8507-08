/**
 * @file Main.cpp
 * @brief 游戏应用程序入口：初始化 NCL 窗口/渲染器/物理，驱动 ECS SceneManager 统一主循环。
 *
 * @details
 * 创建 NCL 核心对象（GameWorld、PhysicsSystem、GameTechRenderer），
 * 构造 ECS::SceneManager 并推入首个场景（Scene_MainMenu）。
 * 主循环按序执行：
 *   1. SceneManager::Update(dt)  — ECS UpdateAll + FixedUpdateAll（累加器）
 *   2. ProcessUIRequests()        — 场景切换、分辨率、全屏、光标
 *   3. NCL world / physics / renderer Update — NCL 层帧更新
 *   4. renderer Render / Present  — 渲染输出
 *   5. SceneManager::EndFrame()  — ProcessPendingDestroy + 延迟场景切换
 */
#include "Window.h"

#include "Debug.h"

#include "StateMachine.h"
#include "StateTransition.h"
#include "State.h"

#include "NavigationGrid.h"
#include "NavigationMesh.h"

#include "TutorialGame.h"

#include "PushdownMachine.h"
#include "PushdownState.h"

#include "BehaviourNode.h"
#include "BehaviourSelector.h"
#include "BehaviourSequence.h"
#include "BehaviourAction.h"

#include "Game/Scenes/Scene_TutorialLevel.h"
#include "Game/Scenes/Scene_HangerA.h"
#include "Game/Scenes/Scene_HangerB.h"
#include "Game/Scenes/Scene_Helipad.h"
#include "Game/Scenes/Scene_Lab.h"
#include "Game/Scenes/Scene_Dock.h"
#include "PhysicsSystem.h"

#ifdef USEOPENGL
#include "GameTechRenderer.h"
#define CAN_COMPILE
#endif
#ifdef USEVULKAN
#include "GameTechVulkanRenderer.h"
#define CAN_COMPILE
#endif

#include "Game/Components/Res_NCL_Pointers.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_Network.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Scenes/SceneManager.h"
#include "Core/Bridge/AssetManager.h"
#include "OGLMesh.h"
#include "Game/Scenes/Scene_PhysicsTest.h"
#include "Game/Scenes/Scene_MainMenu.h"
#include "Game/Scenes/Scene_NavTest.h"
#include "Game/Scenes/Scene_NetworkGame.h"
#include "Game/Components/Res_Input.h"
#include "Game/Components/Res_ScoreConfig.h"
#include "Game/Utils/WindowHelper.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Core/Bridge/ImGuiAdapter.h"
#endif

using namespace NCL;
using namespace CSC8503;

#include <algorithm>
#include <random>
#include <cstring>
#include <thread>
#include <sstream>

#define ENABLE_TUTORIAL_GAME 0  // 1 = 启用遗留 TutorialGame（用于向后兼容测试）

void TestPathfinding() {}
void DisplayPathfinding() {}

// ============================================================
// 辅助函数
// ============================================================

/// 键盘快捷键已迁移到 ECS（Sys_UI 通过 Res_Input 处理）。
/// 原 HandleKeyboardShortcuts() 及 ShowConsole 功能已删除，无实际使用场景。

/// 更新窗口标题
static void UpdateWindowTitle(Window* w, float dt) {
    w->SetTitle("NEUROMANCER - " + std::to_string(1000.0f * dt) + " ms");
}

/// 根据地图 ID 创建对应场景实例
/// Map IDs: 0=HangerA, 1=HangerB, 2=Helipad, 3=Lab, 4=Dock
static IScene* CreateMapScene(uint8_t mapId) {
    switch (mapId) {
        case 0: return new Scene_HangerA();
        case 1: return new Scene_HangerB();
        case 2: return new Scene_Helipad();
        case 3: return new Scene_Lab();
        case 4: return new Scene_Dock();
        default: return new Scene_HangerA();
    }
}

/// 根据 debug 场景索引创建对应场景实例
/// Debug scene IDs: 0=MainMenu, 1=PhysicsTest, 2=NavTest, 3=Tutorial,
/// 4=HangerA, 5=HangerB, 6=Helipad, 7=Lab, 8=Dock, 9=NetworkServer
static IScene* CreateDebugScene(int index) {
    switch (index) {
        case 0: return new Scene_MainMenu();
        case 1: return new Scene_PhysicsTest();
        case 2: return new Scene_NavTest();
        case 3: return new Scene_TutorialLevel();
        case 4: return new Scene_HangerA();
        case 5: return new Scene_HangerB();
        case 6: return new Scene_Helipad();
        case 7: return new Scene_Lab();
        case 8: return new Scene_Dock();
        case 9: return new Scene_NetworkGame(ECS::PeerType::SERVER);
        default: return new Scene_MainMenu();
    }
}

/// 地图名称（调试用）
static const char* kMapNames[] = { "HangerA", "HangerB", "Helipad", "Lab", "Dock" };

/// 重置战役积分到初始状态（由 Res_ScoreConfig 驱动，所有分项归零）。
static void ResetCampaignScore(ECS::Res_UIState& ui, const ECS::Res_ScoreConfig& scoreCfg = {}) {
    ui.campaignScore                = scoreCfg.initialScore;
    ui.scoreDecayAccum              = 0.0f;
    ui.countdownScorePenaltyApplied = false;
    ui.failureScorePenaltyApplied   = false;
    ui.lastScoreRatingTier          = static_cast<int8_t>(ECS::Res_ScoreConfig::RATING_COUNT - 1);
    ui.scoreLost_time      = 0;
    ui.scoreLost_kills     = 0;
    ui.scoreLost_items     = 0;
    ui.scoreLost_countdown = 0;
    ui.scoreLost_failure   = 0;
    ui.scoreKillCount      = 0;
    ui.scoreItemUseCount   = 0;
}

/**
 * @brief 清除当前场景图中的联机模式配置。
 * @details 将 `Res_Network` 标记为离线状态，但不从上下文中移除，交由网络系统统一回收 ENet 资源。
 * @param reg 当前场景注册表
 */
static void ClearNetworkMode(ECS::Registry& reg) {
    if (reg.has_ctx<ECS::Res_Network>()) {
        auto& resNet = reg.ctx<ECS::Res_Network>();
        resNet.mode = ECS::PeerType::OFFLINE;
        resNet.matchSetupReceived = false;
        resNet.bootstrapSceneActive = false;
        resNet.serverIP[0] = '\0';
        resNet.serverPort = 0;
        resNet.preserveSessionOnSceneExit = false;
    }
}

/**
 * @brief 标记当前联机会话将在场景切换时被保留。
 * @details 仅用于多人换关/重开，避免 `Sys_Network::OnDestroy` 将仍在使用的 ENet 会话当成真正断线销毁。
 * @param reg 当前场景注册表
 */
static void PreserveNetworkSession(ECS::Registry& reg) {
    if (reg.has_ctx<ECS::Res_Network>()) {
        auto& resNet = reg.ctx<ECS::Res_Network>();
        if (resNet.mode != ECS::PeerType::OFFLINE) {
            resNet.preserveSessionOnSceneExit = true;
        }
    }
}

/**
 * @brief 以指定角色与地址重建联机配置资源。
 * @details 若 `Res_Network` 已存在则复用并覆盖配置，否则创建新的网络上下文。
 * @param reg 当前场景注册表
 * @param mode 节点网络角色
 * @param ip 目标 IP；服务端模式下仅用于填充默认值
 * @param port 使用的监听或连接端口
 */
static void ConfigureNetworkMode(ECS::Registry& reg,
                                 ECS::PeerType mode,
                                 ECS::MultiplayerMode multiplayerMode,
                                 const char* ip,
                                 uint16_t port) {
    ECS::Res_Network* resNetPtr = nullptr;
    if (reg.has_ctx<ECS::Res_Network>()) {
        resNetPtr = &reg.ctx<ECS::Res_Network>();
    } else {
        resNetPtr = &reg.ctx_emplace<ECS::Res_Network>();
    }

    ECS::Res_Network& resNet = *resNetPtr;
    resNet.mode = mode;
    resNet.multiplayerMode = multiplayerMode;
    resNet.matchSetupReceived = false;
    resNet.bootstrapSceneActive = false;
    strncpy_s(resNet.serverIP, sizeof(resNet.serverIP), ip ? ip : "127.0.0.1", sizeof(resNet.serverIP) - 1);
    resNet.serverPort = port;
    resNet.preserveSessionOnSceneExit = false;
}

/// 使用 std::random_device 获取种子（硬件熵源优先，回退到系统时钟）
static uint32_t TimeBasedSeed() {
    std::random_device rd;
    return rd();
}

/// 随机从 5 张地图中抽 3 张（不排序，打乱顺序直接打），重置 index
static void GenerateMapSequence(ECS::Res_UIState& ui) {
    uint32_t seed = TimeBasedSeed();
    uint8_t pool[] = {0, 1, 2, 3, 4};
    std::mt19937 gen(seed);
    std::shuffle(pool, pool + 5, gen);
    // 取前 3 个，保持 shuffle 后的随机顺序（不排序）
    ui.mapSequence[0] = pool[0];
    ui.mapSequence[1] = pool[1];
    ui.mapSequence[2] = pool[2];
    ui.mapSequenceIndex    = 0;
    ui.mapSequenceGenerated = true;

    std::cout << "[MapSequence] seed=" << seed
              << "  sequence: " << kMapNames[ui.mapSequence[0]]
              << " -> " << kMapNames[ui.mapSequence[1]]
              << " -> " << kMapNames[ui.mapSequence[2]] << std::endl;
    LOG_INFO("[Main] Map sequence generated (seed=" << seed << "): "
             << (int)ui.mapSequence[0] << " -> "
             << (int)ui.mapSequence[1] << " -> "
             << (int)ui.mapSequence[2]);
}

/**
 * @brief 初始化多人模式的随机三关流程。
 * @details Host/Client 各自独立抽取三张地图；比赛只比较三阶段推进速度，不要求同一轮进入相同地图。
 * @param ui 全局 UI 状态资源
 */
static void InitializeMultiplayerMapSequence(ECS::Res_UIState& ui) {
    ui.totalPlayTime = 0.0f;
    ui.debugCurrentScene = -1;
    ResetCampaignScore(ui);
    GenerateMapSequence(ui);
    LOG_INFO("[Main] Multiplayer map sequence initialized independently for this peer.");
}

/**
 * @brief 初始化多人模式 UI 状态。
 * @details 可选择是否立即生成地图序列；同图模式下 Client 会等待服务端下发。
 */
static void InitializeMultiplayerUIState(ECS::Res_UIState& ui, bool generateMapSequence) {
    ui.totalPlayTime = 0.0f;
    ui.debugCurrentScene = -1;
    ui.mapSequenceIndex = 0;
    ui.mapSequenceGenerated = false;
    ResetCampaignScore(ui);
    if (generateMapSequence) {
        GenerateMapSequence(ui);
    }
}

/// 处理所有 UI 请求（场景切换、分辨率、全屏、光标、退出）
static void ProcessUIRequests(ECS::SceneManager& sceneManager, Window* w, bool& running) {
    auto& reg = sceneManager.GetRegistry();

    // 0. Alt+F4 退出（通过 Res_Input 统一检测）
    if (reg.has_ctx<ECS::Res_Input>() && reg.ctx<ECS::Res_Input>().quitRequested) {
        running = false;
        return;
    }

    // 1. Debug 场景切换（优先级最高）
    if (reg.has_ctx<Res_UIFlags>()) {
        auto& flags = reg.ctx<Res_UIFlags>();
        if (flags.debugSceneIndex >= 0) {
            const int debugSceneIndex = flags.debugSceneIndex;
            sceneManager.RequestSceneChange(CreateDebugScene(debugSceneIndex));
            ClearNetworkMode(reg);
            if (reg.has_ctx<ECS::Res_UIState>()) {
                auto& uiDbg = reg.ctx<ECS::Res_UIState>();
                uiDbg.mapSequenceGenerated = false;
                uiDbg.debugCurrentScene    = static_cast<int8_t>(debugSceneIndex);
                uiDbg.pendingSceneRequest  = ECS::SceneRequest::None;
            }
            flags.debugSceneIndex = -1;
        }
    }

    // 2. UI 场景请求
    if (reg.has_ctx<ECS::Res_UIState>()) {
        auto& ui = reg.ctx<ECS::Res_UIState>();

        if (ui.pendingSceneRequest != ECS::SceneRequest::None) {
            switch (ui.pendingSceneRequest) {
                case ECS::SceneRequest::StartGame:
                    ClearNetworkMode(reg);

                    // 正常开始游戏：生成新的 5 抽 3 序列，退出 debug 模式
                    ui.debugCurrentScene = -1;
                    ui.totalPlayTime = 0.0f;
                    ResetCampaignScore(ui);
                    GenerateMapSequence(ui);
                    sceneManager.RequestSceneChange(
                        CreateMapScene(ui.mapSequence[0]));
                    break;
                case ECS::SceneRequest::RestartLevel:
                {
                    const bool isMultiplayer = reg.has_ctx<ECS::Res_Network>()
                        && reg.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;
                    const ECS::MultiplayerMode multiplayerMode = isMultiplayer
                        ? reg.ctx<ECS::Res_Network>().multiplayerMode
                        : ECS::MultiplayerMode::DifferentMapRace;
                    if (ui.debugCurrentScene >= 0) {
                        // Debug 模式：重启当前 debug 场景，不使用地图池
                        if (ui.debugCurrentScene >= 0 && ui.debugCurrentScene <= 8) {
                            ClearNetworkMode(reg);
                        } else if (ui.debugCurrentScene == 9) {
                            PreserveNetworkSession(reg);
                        }
                        sceneManager.RequestSceneChange(CreateDebugScene(ui.debugCurrentScene));
                    } else {
                        // 正常流程：重新随机 5 抽 3 序列，从头开始
                        if (isMultiplayer) {
                            PreserveNetworkSession(reg);
                            if (reg.has_ctx<ECS::Res_GameState>()) {
                                reg.ctx_erase<ECS::Res_GameState>();
                            }
                            if (multiplayerMode == ECS::MultiplayerMode::SameMapGhostRace) {
                                ui.totalPlayTime = 0.0f;
                                ui.mapSequenceIndex = 0;
                                ResetCampaignScore(ui);
                            } else {
                                InitializeMultiplayerMapSequence(ui);
                            }
                        } else {
                            ClearNetworkMode(reg);
                            ui.totalPlayTime = 0.0f;
                            ResetCampaignScore(ui);
                            GenerateMapSequence(ui);
                        }
                        sceneManager.RequestSceneChange(
                            CreateMapScene(ui.mapSequence[0]));
                    }
                    break;
                }
                case ECS::SceneRequest::NextLevel:
                {
                    const bool isMultiplayer = reg.has_ctx<ECS::Res_Network>()
                        && reg.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;
                    if (!isMultiplayer) {
                        ClearNetworkMode(reg);
                    } else {
                        PreserveNetworkSession(reg);
                    }
                    // 序列内前进到下一张地图（积分不重置，仅清除单次惩罚标记）
                    ui.countdownScorePenaltyApplied = false;
                    ui.failureScorePenaltyApplied   = false;
                    ui.mapSequenceIndex++;
                    if (ui.mapSequenceIndex < ECS::Res_UIState::MAP_SEQUENCE_LENGTH) {
                        sceneManager.RequestSceneChange(
                            CreateMapScene(ui.mapSequence[ui.mapSequenceIndex]));
                    } else {
                        // 安全兜底：不应到达这里
                        sceneManager.RequestSceneChange(new Scene_MainMenu());
                    }
                    break;
                }
                case ECS::SceneRequest::LaunchMultiplayerMatch:
                {
                    const bool isMultiplayer = reg.has_ctx<ECS::Res_Network>()
                        && reg.ctx<ECS::Res_Network>().mode != ECS::PeerType::OFFLINE;
                    if (!isMultiplayer || !ui.mapSequenceGenerated) {
                        LOG_WARN("[Main] Ignored LaunchMultiplayerMatch without authoritative map sequence.");
                        break;
                    }

                    PreserveNetworkSession(reg);
                    if (reg.has_ctx<ECS::Res_Network>()) {
                        reg.ctx<ECS::Res_Network>().bootstrapSceneActive = false;
                    }
                    sceneManager.RequestSceneChange(CreateMapScene(ui.mapSequence[ui.mapSequenceIndex]));
                    break;
                }
                case ECS::SceneRequest::ReturnToMenu:
                    ClearNetworkMode(reg);
                    sceneManager.RequestSceneChange(new Scene_MainMenu());
                    break;
                case ECS::SceneRequest::HostGame:
                {
                    uint16_t port = 32499;
                    ECS::MultiplayerMode multiplayerMode = ECS::MultiplayerMode::SameMapGhostRace;
                    if (reg.has_ctx<ECS::Res_LobbyState>()) {
                        port = reg.ctx<ECS::Res_LobbyState>().port;
                        multiplayerMode = reg.ctx<ECS::Res_LobbyState>().multiplayerMode;
                    }
                    ConfigureNetworkMode(reg, ECS::PeerType::SERVER, multiplayerMode, "127.0.0.1", port);
                    if (multiplayerMode == ECS::MultiplayerMode::SameMapGhostRace) {
                        InitializeMultiplayerUIState(ui, true);
                        reg.ctx<ECS::Res_Network>().bootstrapSceneActive = false;
                        sceneManager.RequestSceneChange(CreateMapScene(ui.mapSequence[0]));
                    } else {
                        InitializeMultiplayerMapSequence(ui);
                        sceneManager.RequestSceneChange(CreateMapScene(ui.mapSequence[0]));
                    }
                    break;
                }
                case ECS::SceneRequest::JoinGame: {
                    std::string ip = "127.0.0.1";
                    uint16_t port = 32499;
                    ECS::MultiplayerMode multiplayerMode = ECS::MultiplayerMode::SameMapGhostRace;
                    if (reg.has_ctx<ECS::Res_LobbyState>()) {
                        ip = reg.ctx<ECS::Res_LobbyState>().joinIP;
                        port = reg.ctx<ECS::Res_LobbyState>().port;
                        multiplayerMode = reg.ctx<ECS::Res_LobbyState>().multiplayerMode;
                    }
                    ConfigureNetworkMode(reg, ECS::PeerType::CLIENT, multiplayerMode, ip.c_str(), port);
                    if (multiplayerMode == ECS::MultiplayerMode::SameMapGhostRace) {
                        InitializeMultiplayerUIState(ui, false);
                        reg.ctx<ECS::Res_Network>().bootstrapSceneActive = true;
                        sceneManager.RequestSceneChange(new Scene_NetworkGame(ECS::PeerType::CLIENT, ip, port));
                    } else {
                        InitializeMultiplayerMapSequence(ui);
                        sceneManager.RequestSceneChange(CreateMapScene(ui.mapSequence[0]));
                    }
                    break;
                }
                case ECS::SceneRequest::StartTutorial:
                    ClearNetworkMode(reg);
                    sceneManager.RequestSceneChange(new Scene_TutorialLevel());
                    break;
                case ECS::SceneRequest::QuitApp:
                    running = false;
                    break;
                default:
                    break;
            }
            ui.pendingSceneRequest = ECS::SceneRequest::None;
        }

        // 3. 分辨率切换
        if (ui.resolutionChanged) {
            int idx = std::clamp((int)ui.resolutionIndex, 0, ECS::kResolutionCount - 1);
            WindowHelper::SetWindowSize(
                ECS::kResolutions[idx].width,
                ECS::kResolutions[idx].height);
            ui.resolutionChanged = false;
        }

        // 4. 全屏切换
        if (ui.fullscreenChanged) {
            WindowHelper::SetFullScreen(ui.isFullscreen);
            ui.fullscreenChanged = false;
            if (!ui.isFullscreen) {
                ui.resolutionChanged = true;
            }
        }

        // 5. 光标管理
        w->ShowOSPointer(ui.cursorVisible);
        w->LockMouseToWindow(ui.cursorLocked);
    }
}

// ============================================================
// main 函数
// ============================================================

int main(int argc, char** argv) {

    // =========================================================
    // 窗口初始化
    // =========================================================
    WindowInitialisation initInfo;
    initInfo.width       = 1920;
    initInfo.height      = 1080;
    initInfo.windowTitle = "NEUROMANCER";

    Window* w = Window::CreateGameWindow(initInfo);

    if (!w->HasInitialised()) {
        return -1;
    }

    if (!WindowHelper::Init(w)) {
        LOG_ERROR("[Main] WindowHelper init failed — fullscreen/resolution disabled");
    }

    w->ShowOSPointer(true);
    w->LockMouseToWindow(false);

    // =========================================================
    // 核心系统初始化
    // =========================================================
    GameWorld*     world   = new GameWorld();
    PhysicsSystem* physics = new PhysicsSystem(*world);

#ifdef USEVULKAN
    GameTechVulkanRenderer* renderer = new GameTechVulkanRenderer(*world);
#elif defined(USEOPENGL)
    GameTechRenderer* renderer = new GameTechRenderer(*world);
#endif

    // =========================================================
    // ImGui 初始化
    // =========================================================
#ifdef USE_IMGUI
    ECS::ImGuiAdapter::Init(w, renderer);
#endif

    // 注册窗口 resize 回调，使最大化/拖拽调整大小时 glViewport 跟随更新
    w->SetWindowEventHandler([renderer](NCL::WindowEvent e, uint32_t width, uint32_t height) {
        if (e == NCL::WindowEvent::Resize) {
            renderer->OnWindowResize(width, height);
        }
    });

    // =========================================================
    // 渲染后端工厂注入（ECS 解耦：只有 Main.cpp 知道具体后端类型）
    // =========================================================
    ECS::AssetManager::Instance().SetMeshFactory([]() -> NCL::Rendering::Mesh* {
        return new NCL::Rendering::OGLMesh();
    });

    // =========================================================
    // SceneManager + 统一主循环
    // =========================================================
    {
        ECS::SceneManager sceneManager(Res_NCL_Pointers{ world, physics, renderer });

        // 默认启动场景（MainMenu 提供场景选择入口）
        sceneManager.PushScene(new Scene_MainMenu());

#if ENABLE_TUTORIAL_GAME
        TutorialGame* g = new TutorialGame(*world, *renderer, *physics);
#endif

        bool running = true;
        w->GetTimer().GetTimeDeltaSeconds();

        // ── 统一主循环 ──
        while (w->UpdateWindow() && running) {
            float dt = w->GetTimer().GetTimeDeltaSeconds();
            if (dt > 0.1f) {
                std::cout << "Skipping large time delta" << std::endl;
                continue;
            }

            // 窗口标题更新
            UpdateWindowTitle(w, dt);

#ifdef USE_IMGUI
            ECS::ImGuiAdapter::NewFrame();
#endif

            // ECS 更新
            sceneManager.Update(dt);

            // UI 请求处理（场景切换、分辨率、全屏、光标）
            ProcessUIRequests(sceneManager, w, running);

            // NCL 渲染
            world->UpdateWorld(dt);
            physics->Update(dt);
            renderer->Update(dt);
            renderer->RenderScene();

#ifdef USE_IMGUI
            ECS::ImGuiAdapter::Render();
#endif

            renderer->PresentFrame();

            sceneManager.EndFrame();

            Debug::UpdateRenderables(dt);
        }

        sceneManager.Shutdown();

#if ENABLE_TUTORIAL_GAME
        delete g;
#endif
    }

    // =========================================================
    // 清理
    // =========================================================
    WindowHelper::Shutdown();

#ifdef USE_IMGUI
    ECS::ImGuiAdapter::Shutdown();
#endif

    delete physics;
    delete renderer;
    delete world;

    Window::DestroyGameWindow();

    return 0;
}
