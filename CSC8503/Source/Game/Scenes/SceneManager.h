/**
 * @file SceneManager.h
 * @brief 场景管理器声明。
 *
 * @details
 * 管理场景切换、系统调度、固定步长更新，以及与场景同生命周期的 EventBus。
 */
#pragma once

#include "IScene.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Core/ECS/EventBus.h"
#include "Game/Components/Res_NCL_Pointers.h"
#include <memory>

namespace ECS {

/**
 * @brief 全局场景管理器
 *
 * 持有 ECS::Registry（跨场景共享内存池）和 ECS::SystemManager（每帧调度），
 * 以及当前活跃场景的所有权指针（IScene*）。
 *
 * ## 帧循环接入方式（推荐）
 *
 * @code
 * // 初始化
 * ECS::SceneManager sm(Res_NCL_Pointers{world, physics});
 * sm.PushScene(new Scene_PhysicsTest());
 *
 * // 主循环
 * while (running) {
 *     ImGuiAdapter::NewFrame();
 *     sm.Update(dt);            // ECS UpdateAll + FixedUpdateAll（内含固定步长累加器）
 *
 *     world->UpdateWorld(dt);   // NCL
 *     physics->Update(dt);      // NCL（运行空世界，ECS 实体由 Jolt 管理）
 *     renderer->Update(dt);     // NCL
 *     renderer->Render();       // NCL
 *
 *     sm.EndFrame();            // ProcessPendingDestroy + 延迟场景切换
 *     ImGuiAdapter::Render();   // ImGui
 *     Debug::UpdateRenderables(dt);
 * }
 * sm.Shutdown();
 * @endcode
 *
 * ## 延迟切换机制
 *
 * Scene 内部调用 RequestSceneChange(next)，SceneManager 在 EndFrame() 帧末检查
 * GetNextScene()，执行：CurrentScene->OnExit() → delete CurrentScene
 *                      → CurrentScene = NextScene → NextScene->OnEnter()
 *
 * @see IScene
 */
class SceneManager {
public:
    /**
     * @brief 构造函数：初始化 ECS 环境并预注册 Engine 层全局资源。
     * @param nclPtrs  NCL 引擎指针（跨场景持久化，不随场景销毁）
     */
    explicit SceneManager(Res_NCL_Pointers nclPtrs);

    /** @brief 析构时自动调用 Shutdown()。 */
    ~SceneManager();

    SceneManager(const SceneManager&)            = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // ── 场景控制 ─────────────────────────────────────────────────────────

    /**
     * @brief 进入第一个场景（游戏循环前调用一次）。
     * @param scene 首个场景的所有权（SceneManager 负责 delete）
     */
    void PushScene(IScene* scene);

    /**
     * @brief 每帧前半段：调用所有系统的 OnUpdate，以及固定步长累加器驱动的 FixedUpdateAll。
     * @details UpdateAll 以变步长 dt 调用；FixedUpdateAll 以固定步长 1/60s 调用，
     *          每帧最多步进 4 次（螺旋死亡保护）。EventBus 在所有更新完成后统一 flush。
     * @param dt 本帧变步长时间（秒）
     */
    void Update(float dt);

    /**
     * @brief 每帧后半段（NCL Render 之后调用）：
     *   - 执行 registry.ProcessPendingDestroy()（帧末实体回收）
     *   - 检查并执行延迟场景切换
     */
    void EndFrame();

    /**
     * @brief 从外部请求场景切换（由 Main.cpp 根据 UI 请求调用）。
     * 在 EndFrame() 帧末执行实际切换。
     * @param next 新场景的所有权指针
     */
    void RequestSceneChange(IScene* next);

    /**
     * @brief 关机：安全退出当前场景并释放所有资源。
     * 析构函数自动调用，也可手动提前调用。
     */
    void Shutdown();

    // ── 访问器 ───────────────────────────────────────────────────────────

    ECS::Registry&      GetRegistry() { return m_Registry; }
    ECS::SystemManager& GetSystems()  { return m_Systems;  }

private:
    /**
     * @brief 进入指定场景：创建 EventBus 并注入 ctx，设置 m_CurrentScene，调用 scene->OnEnter()。
     * @details EventBus 在此处由 SceneManager 创建并以裸指针注入 registry ctx，
     *          生命周期与场景对齐，与任何特定 System 解耦；并重置固定步长累加器，
     *          避免跨场景继承上一场景的物理步进残量。
     * @param scene 新场景（所有权已转移）
     */
    void EnterScene(IScene* scene);

    /**
     * @brief 退出当前场景：调用 m_CurrentScene->OnExit()（含 DestroyAll + Clear），
     *        随后清理 EventBus ctx 并销毁 EventBus。
     * @details 退出时同步清零固定步长累加器，防止旧场景残余时间片影响新场景首帧。
     * 调用后 m_CurrentScene 指针被置空，但 delete 由调用方负责。
     */
    void ExitCurrentScene();

    // ── 成员 ─────────────────────────────────────────────────────────────

    ECS::Registry      m_Registry;      ///< ECS 数据源（跨场景共享内存池）
    ECS::SystemManager m_Systems;       ///< 当前帧系统调度器（DestroyAll 后清空）
    IScene*            m_CurrentScene = nullptr; ///< 当前场景（SceneManager 拥有所有权）
    Res_NCL_Pointers   m_NclPtrs;       ///< Engine 层全局资源（不随场景销毁）
    IScene*            m_PendingScene = nullptr; ///< 外部请求的待切换场景
    bool               m_Shutdown = false;       ///< 防止重复 Shutdown
    float              m_FixedAccumulator = 0.0f; ///< 固定步长物理帧累加器（Update 内驱动 FixedUpdateAll）
    std::unique_ptr<ECS::EventBus> m_EventBus;   ///< 场景级事件总线（EnterScene 创建，ExitCurrentScene 销毁）
};

} // namespace ECS
