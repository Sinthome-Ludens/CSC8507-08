/**
 * @file Main.cpp
 * @brief 程序主入口。
 *
 * @details
 * 负责初始化窗口、渲染器与 NCL 运行时资源，并根据编译开关选择启动不同场景流程。
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
#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Init(w, renderer);
#endif

	ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics, renderer});
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

#elif ENABLE_NETWORK_SCENE
        // =========================================================
        // ECS 网络联机场景（SceneManager + Scene_NetworkGame）
        // =========================================================
		const bool runAsServer = (NETWORK_AS_SERVER != 0);
		constexpr float autoExitTime = 0.0f;

#ifdef USE_IMGUI
        ECS::ImGuiAdapter::Init(w, renderer);
#endif

        ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics, renderer});

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
	// 默认 ECS 场景流（主菜单入口）
	// =========================================================

#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Init(w, renderer);
#endif

	ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics, renderer});
	bool running = true;
	sceneManager.PushScene(new Scene_MainMenu());

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

#endif

	delete physics;
	delete renderer;
	delete world;

	Window::DestroyGameWindow();
}
