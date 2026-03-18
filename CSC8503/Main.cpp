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
#include "Game/Components/Res_Network.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Scenes/SceneManager.h"
#include "Game/Scenes/Scene_PhysicsTest.h"
#include "Game/Scenes/Scene_MainMenu.h"
#include "Game/Scenes/Scene_NavTest.h"
#include "Game/Scenes/Scene_NetworkGame.h"
#include "Game/Components/Res_Input.h"
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

/// 处理键盘快捷键
static void HandleKeyboardShortcuts(Window* w) {
    if (Window::GetKeyboard()->KeyPressed(KeyCodes::PRIOR)) {
        w->ShowConsole(true);
    }
    if (Window::GetKeyboard()->KeyPressed(KeyCodes::NEXT)) {
        w->ShowConsole(false);
    }
}

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

/// 地图名称（调试用）
static const char* kMapNames[] = { "HangerA", "HangerB", "Helipad", "Lab", "Dock" };

static void ClearNetworkMode(ECS::Registry& reg) {
    if (reg.has_ctx<ECS::Res_Network>()) {
        reg.ctx_erase<ECS::Res_Network>();
    }
}

static void ConfigureNetworkMode(ECS::Registry& reg, ECS::PeerType mode, const char* ip, uint16_t port) {
    ClearNetworkMode(reg);

    auto& resNet = reg.ctx_emplace<ECS::Res_Network>();
    resNet.mode = mode;
    strncpy_s(resNet.serverIP, sizeof(resNet.serverIP), ip ? ip : "127.0.0.1", sizeof(resNet.serverIP) - 1);
    resNet.serverPort = port;
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
            switch (flags.debugSceneIndex) {
                case 0: sceneManager.RequestSceneChange(new Scene_MainMenu());    break;
                case 1: sceneManager.RequestSceneChange(new Scene_PhysicsTest()); break;
                case 2: sceneManager.RequestSceneChange(new Scene_NavTest());     break;
                case 3: sceneManager.RequestSceneChange(new Scene_TutorialLevel()); break;
                case 4: sceneManager.RequestSceneChange(new Scene_HangerA());     break;
                case 5: sceneManager.RequestSceneChange(new Scene_HangerB());     break;
                case 6: sceneManager.RequestSceneChange(new Scene_Helipad());     break;
                case 7: sceneManager.RequestSceneChange(new Scene_Lab());         break;
                case 8: sceneManager.RequestSceneChange(new Scene_Dock());        break;
                case 9: sceneManager.RequestSceneChange(
                        new Scene_NetworkGame(ECS::PeerType::SERVER));        break;
                default: break;
            }
            if (flags.debugSceneIndex >= 0 && flags.debugSceneIndex <= 8) {
                ClearNetworkMode(reg);
            }
            flags.debugSceneIndex = -1;

            // 清除同帧的 UI 请求
            if (reg.has_ctx<ECS::Res_UIState>()) {
                reg.ctx<ECS::Res_UIState>().pendingSceneRequest = ECS::SceneRequest::None;
            }
        }
    }

    // 2. UI 场景请求
    if (reg.has_ctx<ECS::Res_UIState>()) {
        auto& ui = reg.ctx<ECS::Res_UIState>();

        if (ui.pendingSceneRequest != ECS::SceneRequest::None) {
            switch (ui.pendingSceneRequest) {
                case ECS::SceneRequest::StartGame:
                case ECS::SceneRequest::RestartLevel:
                    ClearNetworkMode(reg);
                    // 每次开始 / 重试都重新随机 5 抽 3 序列
                    GenerateMapSequence(ui);
                    sceneManager.RequestSceneChange(
                        CreateMapScene(ui.mapSequence[0]));
                    break;
                case ECS::SceneRequest::NextLevel:
                    ClearNetworkMode(reg);
                    // 序列内前进到下一张地图
                    ui.mapSequenceIndex++;
                    if (ui.mapSequenceIndex < ECS::Res_UIState::MAP_SEQUENCE_LENGTH) {
                        sceneManager.RequestSceneChange(
                            CreateMapScene(ui.mapSequence[ui.mapSequenceIndex]));
                    } else {
                        // 安全兜底：不应到达这里
                        sceneManager.RequestSceneChange(new Scene_MainMenu());
                    }
                    break;
                case ECS::SceneRequest::ReturnToMenu:
                    ClearNetworkMode(reg);
                    sceneManager.RequestSceneChange(new Scene_MainMenu());
                    break;
                case ECS::SceneRequest::HostGame:
                {
                    uint16_t port = 32499;
                    if (reg.has_ctx<ECS::Res_LobbyState>()) {
                        port = reg.ctx<ECS::Res_LobbyState>().port;
                    }
                    ConfigureNetworkMode(reg, ECS::PeerType::SERVER, "127.0.0.1", port);
                    sceneManager.RequestSceneChange(new Scene_HangerA());
                    break;
                }
                case ECS::SceneRequest::JoinGame: {
                    std::string ip = "127.0.0.1";
                    uint16_t port = 32499;
                    if (reg.has_ctx<ECS::Res_LobbyState>()) {
                        ip = reg.ctx<ECS::Res_LobbyState>().joinIP;
                        port = reg.ctx<ECS::Res_LobbyState>().port;
                    }
                    ConfigureNetworkMode(reg, ECS::PeerType::CLIENT, ip.c_str(), port);
                    sceneManager.RequestSceneChange(new Scene_HangerA());
                    break;
                }
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

            // 键盘快捷键
            HandleKeyboardShortcuts(w);

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
