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

#ifdef USE_IMGUI
#include "Core/Bridge/ImGuiAdapter.h"
#endif

using namespace NCL;
using namespace CSC8503;

#include <chrono>
#include <thread>
#include <sstream>

// ECS Phase 1 验证测试（控制台单元测试）
void RunECSTests();

#define ENABLE_ECS_TEST          0  // 1 = 启用 ECS 单元测试（控制台）
#define ENABLE_PHYSICS_TEST_SCENE 1  // 1 = 启用 ECS 物理测试场景（替换 TutorialGame）

void TestPathfinding() {
}

void DisplayPathfinding() {
}

int main() {
#if ENABLE_ECS_TEST
	RunECSTests();
	std::cout << "\nPress ENTER to continue to game...\n";
	std::cin.get();
#endif

	WindowInitialisation initInfo;
	initInfo.width		= 1920;
	initInfo.height		= 1080;
	initInfo.windowTitle = "NEUROMANCER";

	Window*w = Window::CreateGameWindow(initInfo);

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

#if ENABLE_PHYSICS_TEST_SCENE
	// =========================================================
	// ECS 物理测试场景（SceneManager + Scene_PhysicsTest）
	// =========================================================

#ifdef USE_IMGUI
	ECS::ImGuiAdapter::Init(w, renderer);
#endif

	// SceneManager 持有 Registry + SystemManager，并预注册 Res_NCL_Pointers
	ECS::SceneManager sceneManager(Res_NCL_Pointers{world, physics});

	// 进入首个场景（主菜单）
	sceneManager.PushScene(new Scene_MainMenu());

	w->GetTimer().GetTimeDeltaSeconds();
	bool running = true;
	while (running && w->UpdateWindow()) {
		float dt = w->GetTimer().GetTimeDeltaSeconds();
		if (dt > 0.1f) {
			continue;  // 跳过过大时间增量
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

#ifdef _DEBUG
		w->SetTitle("NEUROMANCER [DEBUG] - " + std::to_string(1000.0f * dt) + " ms");
#endif

#ifdef USE_IMGUI
		ECS::ImGuiAdapter::NewFrame();
#endif

		// 帧前半段：ECS UpdateAll（Camera→Physics→Render→ImGui→UI）
		sceneManager.Update(dt);

		// 处理UI层请求（场景切换 + 窗口操作）
		auto& reg = sceneManager.GetRegistry();
		if (reg.has_ctx<ECS::Res_UIState>()) {
			auto& ui = reg.ctx<ECS::Res_UIState>();

			// ── 场景切换请求 ──
			if (ui.pendingSceneRequest == ECS::SceneRequest::StartGame) {
				ui.pendingSceneRequest = ECS::SceneRequest::None;
				sceneManager.RequestSceneChange(new Scene_PhysicsTest());
			} else if (ui.pendingSceneRequest == ECS::SceneRequest::ReturnToMenu) {
				ui.pendingSceneRequest = ECS::SceneRequest::None;
				sceneManager.RequestSceneChange(new Scene_MainMenu());
			} else if (ui.pendingSceneRequest == ECS::SceneRequest::QuitApp) {
				running = false;
			}

			// ── 全屏切换请求（由 Sys_UI 写入标志，此处执行实际窗口操作）──
			if (ui.fullscreenChanged) {
				ui.fullscreenChanged = false;
				w->SetFullScreen(ui.isFullscreen);
			}

			// ── 鼠标可见性/锁定（每帧根据 UI 状态同步）──
			if (ui.isUIBlockingInput) {
				w->ShowOSPointer(true);
				w->LockMouseToWindow(false);
			} else {
				w->ShowOSPointer(false);
				w->LockMouseToWindow(true);
			}
		}

		// NCL 渲染（GameTechRenderer 自动渲染 GameWorld 中的所有代理对象）
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

#else
	// =========================================================
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
	nclPtrs.world   = world;
	nclPtrs.physics = physics;
	imguiRegistry.ctx_emplace<Res_NCL_Pointers>(nclPtrs);
#endif

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

		w->SetTitle("Gametech frame time:" + std::to_string(1000.0f * dt));

#ifdef USE_IMGUI
		ECS::ImGuiAdapter::NewFrame();
		imguiSystems.UpdateAll(imguiRegistry, dt);
#endif

		g->UpdateGame(dt);

		world->UpdateWorld(dt);
		physics->Update(dt);
		renderer->Update(dt);
		renderer->Render();

#ifdef USE_IMGUI
		ECS::ImGuiAdapter::Render();
#endif

		Debug::UpdateRenderables(dt);
	}

#ifdef USE_IMGUI
	imguiSystems.DestroyAll(imguiRegistry);
	ECS::ImGuiAdapter::Shutdown();
#endif

#endif  // ENABLE_PHYSICS_TEST_SCENE

	Window::DestroyGameWindow();
}
