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
#include "Game/Scenes/SceneManager.h"
#include "Game/Scenes/Scene_PhysicsTest.h"
#include "Game/Scenes/Scene_NetworkGame.h"

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
#define ENABLE_PHYSICS_TEST_SCENE 0  // 1 = 启用 ECS 物理测试场景（替换 TutorialGame）
#define ENABLE_NETWORK_SCENE      1  // 1 = 启用 ECS 网络联机场景
#define NETWORK_AS_SERVER         1  // 1 = 以服务器模式启动，0 = 以客户端模式启动

void TestPathfinding() {
}

void DisplayPathfinding() {
}

int main(int argc, char** argv) {
    bool runAsServer = true;
    if (argc > 1 && std::string(argv[1]) == "client") {
        runAsServer = false;
    }
    float autoExitTime = 0.0f;
    if (argc > 2) {
        autoExitTime = std::stof(argv[2]);
    }

#if ENABLE_ECS_TEST
	RunECSTests();
	std::cout << "\nPress ENTER to continue to game...\n";
	std::cin.get();
#endif

	WindowInitialisation initInfo;
	initInfo.width		= 1280;
	initInfo.height		= 720;
	initInfo.windowTitle = "CSC8503 Game technology!";

	Window*w = Window::CreateGameWindow(initInfo);

	if (!w->HasInitialised()) {
		return -1;
	}

	w->ShowOSPointer(false);
	w->LockMouseToWindow(true);

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

        // 进入首个场景（OnEnter：加载资源 → 创建初始实体 → AwakeAll）
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

                w->SetTitle("ECS Physics Test - frame time: " + std::to_string(1000.0f * dt) + " ms");    

#ifdef USE_IMGUI
                ECS::ImGuiAdapter::NewFrame();
#endif

                // 帧前半段：ECS UpdateAll（Camera→Physics→Render→ImGui）
                sceneManager.Update(dt);

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
