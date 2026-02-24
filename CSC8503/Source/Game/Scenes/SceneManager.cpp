#include "SceneManager.h"
#include "Game/Utils/Assert.h"
#include "Game/Utils/Log.h"

namespace ECS {

// ============================================================
// Constructor / Destructor
// ============================================================

SceneManager::SceneManager(Res_NCL_Pointers nclPtrs)
    : m_NclPtrs(nclPtrs)
{
    // Res_NCL_Pointers 属于 Engine 层全局资源，跨场景持久化
    // 在构造时一次性注册，所有场景的 System 均可通过 ctx<Res_NCL_Pointers>() 访问
    m_Registry.ctx_emplace<Res_NCL_Pointers>(m_NclPtrs);

    LOG_INFO("[SceneManager] Initialized. Res_NCL_Pointers registered to context.");
}

SceneManager::~SceneManager() {
    Shutdown();
}

// ============================================================
// PushScene
// ============================================================

void SceneManager::PushScene(IScene* scene) {
    GAME_ASSERT(scene != nullptr,
                "[SceneManager] PushScene: scene pointer is null.");
    GAME_ASSERT(m_CurrentScene == nullptr,
                "[SceneManager] PushScene: a scene is already active. "
                "Call Shutdown() before pushing a new root scene.");

    EnterScene(scene);
}

// ============================================================
// Update（帧前半段：ECS UpdateAll）
// ============================================================

void SceneManager::Update(float dt) {
    if (!m_CurrentScene || m_Shutdown) return;

    m_Systems.UpdateAll(m_Registry, dt);
}

// ============================================================
// EndFrame（帧后半段：ProcessPendingDestroy + 延迟场景切换）
// ============================================================

void SceneManager::EndFrame() {
    if (!m_CurrentScene || m_Shutdown) return;

    // 1. 帧末实体回收（在 NCL Render 之后调用，确保实体在本帧渲染完成后再销毁）
    m_Registry.ProcessPendingDestroy();

    // 2. 延迟场景切换（安全序列：检查 → OnExit → delete → OnEnter）
    //    优先检查外部请求（m_PendingScene），再检查场景内部请求
    IScene* next = m_CurrentScene->GetNextScene();
    if (!next && m_PendingScene) {
        next = m_PendingScene;
        m_PendingScene = nullptr;
    }
    if (next) {
        m_CurrentScene->ClearNextScene();

        IScene* old = m_CurrentScene;
        m_CurrentScene = nullptr;   // 先置空，避免 ExitCurrentScene 内部误读
        old->OnExit(m_Registry, m_Systems);
        LOG_INFO("[SceneManager] Scene exited (deferred switch).");
        delete old;

        // 清空所有实体与组件池，防止上一关状态污染下一关。
        // Context 不受影响（Res_NCL_Pointers 等引擎级资源跨场景持久化）。
        // 场景级 Context 由各 Scene::OnEnter / System::OnAwake 用 ctx_emplace 刷新。
        m_Registry.Clear();

        EnterScene(next);
    }
}

// ============================================================
// RequestSceneChange（外部请求）
// ============================================================

void SceneManager::RequestSceneChange(IScene* next) {
    if (m_PendingScene && m_PendingScene != next) {
        LOG_WARN("[SceneManager] Overwriting pending scene request! Deleting previous.");
        delete m_PendingScene;
    }
    m_PendingScene = next;
}

// ============================================================
// Shutdown
// ============================================================

void SceneManager::Shutdown() {
    if (m_Shutdown) return;
    m_Shutdown = true;

    if (m_CurrentScene) {
        IScene* old = m_CurrentScene;
        ExitCurrentScene();   // 内部置 m_CurrentScene = nullptr
        delete old;           // 正确释放场景对象
        LOG_INFO("[SceneManager] Shutdown complete.");
    }
}

// ============================================================
// Private: EnterScene
// ============================================================

void SceneManager::EnterScene(IScene* scene) {
    m_CurrentScene = scene;
    m_CurrentScene->OnEnter(m_Registry, m_Systems, m_NclPtrs);
    LOG_INFO("[SceneManager] Scene entered. Active systems: " << m_Systems.Count());
}

// ============================================================
// Private: ExitCurrentScene
// ============================================================

void SceneManager::ExitCurrentScene() {
    if (!m_CurrentScene) return;
    m_CurrentScene->OnExit(m_Registry, m_Systems);
    m_CurrentScene = nullptr;
    LOG_INFO("[SceneManager] Scene exited.");
}

} // namespace ECS
