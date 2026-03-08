/**
 * @file Main.cpp
 * @brief 游戏应用程序入口：初始化 NCL 窗口/渲染器/物理，驱动 ECS SceneManager 主循环。
 *
 * @details
 * 创建 NCL 核心对象（GameWorld、PhysicsSystem、GameTechRenderer），
 * 构造 ECS::SceneManager 并推入首个场景（Scene_PhysicsTest）。
 * 主循环按序执行：
 *   1. SceneManager::Update(dt)  — ECS UpdateAll + FixedUpdateAll（累加器）
 *   2. NCL world/physics/renderer Update — NCL 层帧更新
 *   3. renderer Render/Present    — 渲染输出
 *   4. SceneManager::EndFrame()  — ProcessPendingDestroy + 延迟场景切换
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
#include "Game/Components/Res_UIState.h"
#include "Game/Components/Res_UIFlags.h"
#include "Game/Components/Res_LobbyState.h"
#include "Game/Scenes/SceneManager.h"
#include "Game/Scenes/Scene_PhysicsTest.h"
#include "Game/Scenes/Scene_MainMenu.h"
#include "Game/Scenes/Scene_NavTest.h"
#include "Game/Scenes/Scene_NetworkGame.h"
#include "Game/Utils/WindowHelper.h"
#include "Game/Utils/Log.h"

#ifdef USE_IMGUI
#include "Core/Bridge/ImGuiAdapter.h"
#endif

using namespace NCL;
using namespace CSC8503;

#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>

// ECS Phase 1 验证测试（控制台单元测试）
void RunECSTests();

#define ENABLE_ECS_TEST          0  // 1 = 启用 ECS 单元测试（控制台）
#define ENABLE_PHYSICS_TEST_SCENE 1  // 1 = 启用 ECS 物理测试场景（替换 TutorialGame）
#define ENABLE_NETWORK_SCENE      0  // 1 = 启用 ECS 网络联机场景
#define NETWORK_AS_SERVER         1  // 1 = 以服务器模式启动，0 = 以客户端模式启动

void TestPathfinding() {
}

void DisplayPathfinding() {
}

/**
 * @brief 应用程序入口：初始化所有子系统，运行主游戏循环，退出时释放资源。
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 0 表示正常退出，-1 表示窗口初始化失败
 */
int main(int argc, char** argv) {
	WindowInitialisation initInfo;
	initInfo.width		= 1920;
	initInfo.height		= 1080;
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

	GameWorld* world = new GameWorld();
	PhysicsSystem* physics = new PhysicsSystem(*world);

#ifdef USEVULKAN
	GameTechVulkanRenderer* renderer = new GameTechVulkanRenderer(*world);
#elif USEOPENGL
	GameTechRenderer* renderer = new GameTechRenderer(*world);
#endif

#if ENABLE_PHYSICS_TEST_SCENE
    // =========================================================
    // ECS 物理测试场景（SceneManager + Scene_PhysicsTest）
    // =========================================================

#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Init(w, renderer);
#endif

	ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics, renderer});

	bool running = true;
	sceneManager.PushScene(new Scene_PhysicsTest());

    w->GetTimer().GetTimeDeltaSeconds();
    while (w->UpdateWindow() && !Window::GetKeyboard()->KeyDown(KeyCodes::ESCAPE)) {
            float dt = w->GetTimer().GetTimeDeltaSeconds();
            if (dt > 0.1f) {
                    std::cout << "Skipping large time delta" << std::endl;
                    continue;
            }
            if (Window::GetKeyboard()->KeyPressed(KeyCodes::PRIOR)) {
                    w->ShowConsole(true);
            }
            if (Window::GetKeyboard()->KeyPressed(KeyCodes::NEXT)) {
                    w->ShowConsole(false);
            }
            if (Window::GetKeyboard()->KeyPressed(KeyCodes::T)) {
                    w->SetWindowPosition(0, 0);
            }

            w->SetTitle("ECS PhysicsTest - frame time: " + std::to_string(1000.0f * dt) + " ms");    

#ifdef USE_IMGUI
            ECS::ImGuiAdapter::NewFrame();
#endif

            // 帧前半段：ECS UpdateAll（Camera→Physics→Render→ImGui）
            sceneManager.Update(dt);

            // NCL 渲染（GameTechRenderer 自动渲染 GameWorld 中的所有代理对象）
            // 合并：保留 master 分支的渲染逻辑
            world->UpdateWorld(dt);
            physics->Update(dt);   // NCL 物理运行空世界（ECS 实体由 Jolt 管理）
            renderer->Update(dt);
            renderer->RenderScene();   // BeginFrame + RenderFrame + EndFrame（不交换缓冲区）

#ifdef USE_IMGUI
            ECS::ImGuiAdapter::Render(); // ImGui 叠加在 3D 场景之上，swap 之前渲染
#endif

            renderer->PresentFrame();  // 呈现完整帧（3D + ImGui）

            // 帧后半段：ProcessPendingDestroy + 延迟场景切换检测
            sceneManager.EndFrame();

            Debug::UpdateRenderables(dt);
    }

    // 安全退出当前场景（OnExit → DestroyAll → delete）
    sceneManager.Shutdown();

#ifdef USE_IMGUI
    ECS::ImGuiAdapter::Shutdown();
#endif

#elif ENABLE_NETWORK_SCENE
        // =========================================================
        // ECS 网络联机场景（SceneManager + Scene_NetworkGame）
        // =========================================================

#ifdef USE_IMGUI
        ECS::ImGuiAdapter::Init(w, renderer);
#endif

        ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics});

        if (runAsServer) {
            sceneManager.PushScene(new Scene_NetworkGame(ECS::PeerType::SERVER));
        } else {
            sceneManager.PushScene(new Scene_NetworkGame(ECS::PeerType::CLIENT, "127.0.0.1"));
        }

        w->GetTimer().GetTimeDeltaSeconds();
        float autoExitTimer = 0.0f;
        while (w->UpdateWindow() && !Window::GetKeyboard()->KeyDown(KeyCodes::ESCAPE)) {
                float dt = w->GetTimer().GetTimeDeltaSeconds();
                if (dt > 0.1f) continue;
                
                if (autoExitTime > 0.0f) {
                    autoExitTimer += dt;
                    if (autoExitTimer > autoExitTime) break;
                }

                w->SetTitle(std::string("ECS Network Test - ") + (runAsServer ? "SERVER" : "CLIENT") + " - " + std::to_string(1.0f/dt) + " FPS");

#ifdef USE_IMGUI
                ECS::ImGuiAdapter::NewFrame();
#endif

                sceneManager.Update(dt);

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

#ifdef USE_IMGUI
        ECS::ImGuiAdapter::Shutdown();
#endif

#else	// =========================================================
	// 原 TutorialGame 逻辑（ENABLE_PHYSICS_TEST_SCENE == 0 时）
	// =========================================================
	TutorialGame* g = new TutorialGame(*world, *renderer, *physics);

#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Init(w, renderer);
	ECS::Registry      imguiRegistry;
	ECS::SystemManager imguiSystems;
	imguiSystems.Register<ECS::Sys_ImGui>(1000);
	imguiSystems.AwakeAll(imguiRegistry);

	Res_NCL_Pointers nclPtrs;
	nclPtrs.world    = world;
	nclPtrs.physics  = physics;
	nclPtrs.renderer = renderer;
	imguiRegistry.ctx_emplace<Res_NCL_Pointers>(nclPtrs);
#endif

	w->GetTimer().GetTimeDeltaSeconds();

	while (w->UpdateWindow() && running) {
		float dt = w->GetTimer().GetTimeDeltaSeconds();
		if (dt > 0.1f) {
			continue;
		}

		if (Window::GetKeyboard()->KeyPressed(KeyCodes::PRIOR)) {
			w->ShowConsole(true);
		}
		if (Window::GetKeyboard()->KeyPressed(KeyCodes::NEXT)) {
			w->ShowConsole(false);
		}

#ifdef USE_IMGUI
		ECS::ImGuiAdapter::NewFrame();
#endif

		sceneManager.Update(dt);

		// ── UI request processing ──
		{
			auto& reg = sceneManager.GetRegistry();

			// Debug scene selector (优先：调试覆盖优先于 UI 场景请求)
			if (reg.has_ctx<Res_UIFlags>()) {
				auto& flags = reg.ctx<Res_UIFlags>();
				if (flags.debugSceneIndex >= 0) {
					switch (flags.debugSceneIndex) {
						case 0: sceneManager.RequestSceneChange(new Scene_MainMenu());     break;
						case 1: sceneManager.RequestSceneChange(new Scene_PhysicsTest());  break;
						case 2: sceneManager.RequestSceneChange(new Scene_NavTest());      break;
						case 3: sceneManager.RequestSceneChange(new Scene_NetworkGame(ECS::PeerType::SERVER)); break;
					}
					flags.debugSceneIndex = -1;

					// 清除同帧的 UI 请求，避免分配后立即被覆盖删除
					if (reg.has_ctx<ECS::Res_UIState>()) {
						reg.ctx<ECS::Res_UIState>().pendingSceneRequest = ECS::SceneRequest::None;
					}
				}
			}

			if (reg.has_ctx<ECS::Res_UIState>()) {
				auto& ui = reg.ctx<ECS::Res_UIState>();

				// Scene change requests
				if (ui.pendingSceneRequest != ECS::SceneRequest::None) {
					switch (ui.pendingSceneRequest) {
						case ECS::SceneRequest::StartGame:
							sceneManager.RequestSceneChange(new Scene_PhysicsTest());
							break;
						case ECS::SceneRequest::RestartLevel:
							sceneManager.RequestSceneChange(new Scene_PhysicsTest());
							break;
						case ECS::SceneRequest::ReturnToMenu:
							sceneManager.RequestSceneChange(new Scene_MainMenu());
							break;
						case ECS::SceneRequest::HostGame:
							sceneManager.RequestSceneChange(
								new Scene_NetworkGame(ECS::PeerType::SERVER));
							break;
						case ECS::SceneRequest::JoinGame: {
							std::string ip = "127.0.0.1";
							if (reg.has_ctx<ECS::Res_LobbyState>()) {
								ip = reg.ctx<ECS::Res_LobbyState>().joinIP;
							}
							sceneManager.RequestSceneChange(
								new Scene_NetworkGame(ECS::PeerType::CLIENT, ip));
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

				// Resolution change
				if (ui.resolutionChanged) {
					int idx = std::clamp((int)ui.resolutionIndex, 0, ECS::kResolutionCount - 1);
					WindowHelper::SetWindowSize(ECS::kResolutions[idx].width, ECS::kResolutions[idx].height);
					ui.resolutionChanged = false;
				}

				// Fullscreen toggle
				if (ui.fullscreenChanged) {
					WindowHelper::SetFullScreen(ui.isFullscreen);
					ui.fullscreenChanged = false;
					if (!ui.isFullscreen) {
						ui.resolutionChanged = true;  // sync window to resolutionIndex on exit
					}
				}

				// Cursor management (driven by Sys_UI flags)
				w->ShowOSPointer(ui.cursorVisible);
				w->LockMouseToWindow(ui.cursorLocked);
			}
		}

		// NCL rendering
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

#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Shutdown();
#endif

	delete physics;
	delete renderer;
	delete world;

	Window::DestroyGameWindow();

#endif // ENABLE_PHYSICS_TEST_SCENE / ENABLE_NETWORK_SCENE / else
}
