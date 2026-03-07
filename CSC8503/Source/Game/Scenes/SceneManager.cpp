#include "SceneManager.h"
#include "Game/Utils/Assert.h"
#include "Game/Utils/Log.h"
#include "Game/Components/Res_Time.h"
#include "Core/ECS/EventBus.h"

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

    // 更新全局时间资源 Res_Time
    if (!m_Registry.has_ctx<Res_Time>()) {
        m_Registry.ctx_emplace<Res_Time>();
    }
    auto& time = m_Registry.ctx<Res_Time>();
    time.deltaTime = dt;
    time.totalTime += dt * time.timeScale;
    time.frameCount++;

    m_Systems.UpdateAll(m_Registry, dt);
    // 修复：刷新延迟事件队列
    if (m_Registry.has_ctx<EventBus*>()) {
        auto* eventBus = m_Registry.ctx<EventBus*>();
        if (eventBus) {
            eventBus->flush();
        }
    }
}

// ============================================================
// EndFrame（帧后半段：ProcessPendingDestroy + 延迟场景切换）
// ============================================================

void SceneManager::EndFrame() {
    if (!m_CurrentScene || m_Shutdown) return;

    // 1. 帧末实体回收（在 NCL Render 之后调用，确保实体在本帧渲染完成后再销毁）
    m_Registry.ProcessPendingDestroy();

    // 2. 延迟场景切换（安全序列：检查 → OnExit → delete → OnEnter）
    //    优先检查外部请求（Main.cpp），其次检查内部请求（IScene::RequestSceneChange）
    IScene* next = m_PendingScene ? m_PendingScene : m_CurrentScene->GetNextScene();
    if (next) {
        // M7修复：外部请求优先时，释放内部请求避免泄漏
        if (m_PendingScene) {
            IScene* internal = m_CurrentScene->GetNextScene();
            if (internal && internal != m_PendingScene) {
                delete internal;
            }
        }
        m_PendingScene = nullptr;
        m_CurrentScene->ClearNextScene();

        IScene* old = m_CurrentScene;
        m_CurrentScene = nullptr;
        old->OnExit(m_Registry, m_Systems);
        LOG_INFO("[SceneManager] Scene exited (deferred switch).");
        delete old;

        EnterScene(next);
    }
}

// ============================================================
// RequestSceneChange
// ============================================================

void SceneManager::RequestSceneChange(IScene* next) {
    // 防止同帧多次请求导致前一个分配泄漏
    if (m_PendingScene && m_PendingScene != next) {
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
        ExitCurrentScene();   // OnExit + 置空 m_CurrentScene
        delete old;
        LOG_INFO("[SceneManager] Shutdown complete.");
    }

    // 释放未处理的待切换场景（C4修复）
    delete m_PendingScene;
    m_PendingScene = nullptr;
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
