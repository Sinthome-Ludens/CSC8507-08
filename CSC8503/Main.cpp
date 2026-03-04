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
						case ECS::SceneRequest::QuitApp:
							running = false;
							break;
						default:
							break;
					}
					ui.pendingSceneRequest = ECS::SceneRequest::None;
				}

				// Fullscreen toggle
				if (ui.fullscreenChanged) {
					w->SetFullScreen(ui.isFullscreen);
					ui.fullscreenChanged = false;
				}

				// Mouse management: show cursor when UI is blocking input
				w->ShowOSPointer(ui.isUIBlockingInput);
				w->LockMouseToWindow(!ui.isUIBlockingInput);
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

	Window::DestroyGameWindow();
}
