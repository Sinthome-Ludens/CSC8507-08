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

#ifdef USE_IMGUI
#include "Core/Bridge/ImGuiAdapter.h"
#endif

using namespace NCL;
using namespace CSC8503;

#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>

void TestPathfinding() {
}

void DisplayPathfinding() {
}

int main(int argc, char** argv) {
	WindowInitialisation initInfo;
	initInfo.width		= 1920;
	initInfo.height		= 1080;
	initInfo.windowTitle = "NEUROMANCER";

	Window* w = Window::CreateGameWindow(initInfo);

	if (!w->HasInitialised()) {
		return -1;
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

#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Init(w, renderer);
#endif

	ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics, renderer});
	sceneManager.PushScene(new Scene_MainMenu());

	bool running = true;
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
					static constexpr int kResWidths[]  = { 1280, 1600, 1920 };
					static constexpr int kResHeights[] = { 720,  900,  1080 };
					int idx = std::clamp((int)ui.resolutionIndex, 0, 2);
					w->SetWindowSize(kResWidths[idx], kResHeights[idx]);
					ui.resolutionChanged = false;
				}

				// Fullscreen toggle
				if (ui.fullscreenChanged) {
					w->SetFullScreen(ui.isFullscreen);
					ui.fullscreenChanged = false;
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
}
