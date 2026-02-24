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

    // 先执行变步长系统（输入、相机、UI 等）
    m_Systems.UpdateAll(m_Registry, dt);

    // 再执行固定步长系统（物理等），统一由 SceneManager 调度入口
    if (dt <= 0.0f) return;

    m_FixedAccumulator += dt;
    int fixedSteps = 0;
    while (m_FixedAccumulator >= m_FixedDt && fixedSteps < m_MaxFixedStepsPerFrame) {
        m_Systems.FixedUpdateAll(m_Registry, m_FixedDt);
        m_FixedAccumulator -= m_FixedDt;
        ++fixedSteps;
    }

    // 防止极端情况下累计残值无限增长（如长时间卡顿）
    if (fixedSteps == m_MaxFixedStepsPerFrame && m_FixedAccumulator > m_FixedDt) {
        m_FixedAccumulator = m_FixedDt;
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
    IScene* next = m_CurrentScene->GetNextScene();
    if (next) {
        m_CurrentScene->ClearNextScene();

        IScene* old = m_CurrentScene;
        m_CurrentScene = nullptr;   // 先置空，避免 ExitCurrentScene 内部误读
        old->OnExit(m_Registry, m_Systems);
        LOG_INFO("[SceneManager] Scene exited (deferred switch).");
        delete old;

        EnterScene(next);
    }
}

// ============================================================
// Shutdown
// ============================================================

void SceneManager::Shutdown() {
    if (m_Shutdown) return;
    m_Shutdown = true;

    // 关机时重置固定步长累加器，避免下一次初始化沿用旧值
    m_FixedAccumulator = 0.0f;

    if (m_CurrentScene) {
        ExitCurrentScene();
        delete m_CurrentScene;
        m_CurrentScene = nullptr;
        LOG_INFO("[SceneManager] Shutdown complete.");
    }
}

// ============================================================
// Private: EnterScene
// ============================================================

void SceneManager::EnterScene(IScene* scene) {
    m_CurrentScene = scene;
    // 进入新场景前清空累加器，防止上一场景的时间残值影响本场景
    m_FixedAccumulator = 0.0f;
    m_CurrentScene->OnEnter(m_Registry, m_Systems, m_NclPtrs);
    LOG_INFO("[SceneManager] Scene entered. Active systems: " << m_Systems.Count());
}

// ============================================================
// Private: ExitCurrentScene
// ============================================================

void SceneManager::ExitCurrentScene() {
    if (!m_CurrentScene) return;

    // 退出当前场景时清空累加器，避免残值跨场景传播
    m_FixedAccumulator = 0.0f;
    m_CurrentScene->OnExit(m_Registry, m_Systems);
    m_CurrentScene = nullptr;
    LOG_INFO("[SceneManager] Scene exited.");
}

} // namespace ECS
