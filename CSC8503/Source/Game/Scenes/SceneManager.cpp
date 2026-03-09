/**
 * @file SceneManager.cpp
 * @brief ECS 场景管理器实现：帧循环驱动、场景生命周期与固定步长累加器。
 *
 * @details
 * - `Update(dt)`：驱动变步长 UpdateAll 与固定步长 FixedUpdateAll（累加器模式）。
 *   固定步长从 `Res_Time::fixedDeltaTime` 读取，保证单一数据来源。
 *   累加器上限 clamp 为 4 × fixedDt，防止过载导致螺旋死亡。
 * - `EnterScene()` / `ExitCurrentScene()`：切换场景时重置累加器，
 *   防止残留积压时间在新场景首帧触发大量物理步进。
 * - `EndFrame()`：帧末执行 ProcessPendingDestroy 与延迟场景切换。
 */
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

/**
 * @brief 每帧前半段：驱动变步长 UpdateAll 与固定步长 FixedUpdateAll。
 * @details
 * 1. 更新 Res_Time（deltaTime / totalTime / frameCount）。
 * 2. 调用 UpdateAll(dt)（所有系统 OnUpdate，变步长）。
 * 3. 从 Res_Time::fixedDeltaTime 读取固定步长，累加器积分；
 *    每帧最多步进 4 次（螺旋死亡保护），并 clamp 累加器上限。
 * 4. 刷新 EventBus（所有更新完成后统一 flush）。
 * @param dt 本帧变步长时间（秒）
 */
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

    // 固定步长物理帧驱动（由 Res_Time::fixedDeltaTime 决定，最多步进 4 次防止螺旋死亡）
    // 使用 Res_Time 作为单一数据来源，与 Sys_Physics::FIXED_DT 保持一致
    const float fixedDt = time.fixedDeltaTime;
    m_FixedAccumulator += dt;
    // clamp：防止过载时累加器无界积压（上限 = 4 步）
    if (m_FixedAccumulator > fixedDt * 4.0f) {
        m_FixedAccumulator = fixedDt * 4.0f;
    }
    int steps = 0;
    while (m_FixedAccumulator >= fixedDt && steps < 4) {
        m_Systems.FixedUpdateAll(m_Registry, fixedDt);
        m_FixedAccumulator -= fixedDt;
        ++steps;
    }

    // 刷新延迟事件队列（在所有更新完成后统一 flush）
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

/**
 * @brief 进入指定场景：重置累加器，设置 m_CurrentScene 并调用 OnEnter。
 * @details
 * 累加器在 OnEnter 之前置零，防止上一场景残留的积压时间在新场景首帧触发大量物理步进。
 * @param scene 新场景（所有权已转移至 SceneManager）
 */
void SceneManager::EnterScene(IScene* scene) {
    // 重置累加器，防止上一场景残留的积压时间在新场景首帧触发大量物理步进
    m_FixedAccumulator = 0.0f;
    m_CurrentScene = scene;
    m_CurrentScene->OnEnter(m_Registry, m_Systems, m_NclPtrs);
    LOG_INFO("[SceneManager] Scene entered. Active systems: " << m_Systems.Count());
}

// ============================================================
// Private: ExitCurrentScene
// ============================================================

/**
 * @brief 退出当前场景：调用 OnExit，置空 m_CurrentScene，重置累加器。
 * @details
 * 累加器在 OnExit 之后置零，防止场景切换后残留积压时间干扰下一场景的物理步进。
 * delete 由调用方（PushScene / EndFrame / Shutdown）负责。
 */
void SceneManager::ExitCurrentScene() {
    if (!m_CurrentScene) return;
    m_CurrentScene->OnExit(m_Registry, m_Systems);
    m_CurrentScene = nullptr;
    // 重置累加器，防止场景切换后残留积压时间干扰下一场景的物理步进
    m_FixedAccumulator = 0.0f;
    LOG_INFO("[SceneManager] Scene exited.");
}

} // namespace ECS
