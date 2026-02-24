#pragma once

#include "IScene.h"
#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_NCL_Pointers.h"

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
 *     sm.Update(dt);            // ECS UpdateAll
 *
 *     world->UpdateWorld(dt);   // NCL
 *     physics->Update(dt);      // NCL
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
     * @brief 每帧前半段：调用所有系统的 OnUpdate（含 ImGui UI 收集）。
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
     * @brief 关机：安全退出当前场景并释放所有资源。
     * 析构函数自动调用，也可手动提前调用。
     */
    void Shutdown();

    // ── 访问器 ───────────────────────────────────────────────────────────

    ECS::Registry&      GetRegistry() { return m_Registry; }
    ECS::SystemManager& GetSystems()  { return m_Systems;  }

private:
    /**
     * @brief 进入指定场景：设置 m_CurrentScene 并调用 scene->OnEnter()。
     * @param scene 新场景（所有权已转移）
     */
    void EnterScene(IScene* scene);

    /**
     * @brief 退出当前场景：调用 m_CurrentScene->OnExit()（含 DestroyAll + Clear）。
     * 调用后 m_CurrentScene 指针被置空，但 delete 由调用方负责。
     */
    void ExitCurrentScene();

    // ── 成员 ─────────────────────────────────────────────────────────────

    ECS::Registry      m_Registry;      ///< ECS 数据源（跨场景共享内存池）
    ECS::SystemManager m_Systems;       ///< 当前帧系统调度器（DestroyAll 后清空）
    IScene*            m_CurrentScene = nullptr; ///< 当前场景（SceneManager 拥有所有权）
    Res_NCL_Pointers   m_NclPtrs;       ///< Engine 层全局资源（不随场景销毁）
    bool               m_Shutdown = false;       ///< 防止重复 Shutdown

    // 固定步长调度状态（由 Update 中的累加器循环使用）
    float m_FixedAccumulator = 0.0f;    ///< 固定步长累加器
    float m_FixedDt = 1.0f / 60.0f;     ///< 固定物理步长（默认 60Hz）
    int   m_MaxFixedStepsPerFrame = 4;  ///< 单帧最大固定步进次数，防止螺旋死亡
};

} // namespace ECS
