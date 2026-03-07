#pragma once

#include "Core/ECS/Registry.h"
#include "Core/ECS/SystemManager.h"
#include "Game/Components/Res_NCL_Pointers.h"

/**
 * @brief 场景抽象接口
 *
 * 场景是"当前生效的系统集合"——每个场景定义加载哪些 System、哪些资源。
 * 具体场景（如 Scene_PhysicsTest、Scene_Gameplay）继承此接口并实现
 * OnEnter / OnExit 两个生命周期钩子。
 *
 * 生命周期（按 游戏开发.md §3.3.2）：
 *   Construction → OnEnter → [Active Loop] → OnExit → Destruction
 *
 * 场景切换（延迟机制）：
 *   Scene 内部调用 RequestSceneChange(next)，SceneManager 在帧末检查
 *   GetNextScene() 并执行 OnExit → delete current → OnEnter next 的安全序列。
 *
 * @see SceneManager  (持有并调度 IScene)
 */
class IScene {
public:
    virtual ~IScene() = default;

    /**
     * @brief 场景加载阶段（由 SceneManager 在首帧调用）
     *
     * 应在此处：
     *   - 向 Registry 注册全局资源（ctx_emplace）
     *   - 向 SystemManager 注册并启动所有 System
     *   - 调用 systems.AwakeAll(registry)
     *
     * @param registry ECS 注册表（跨场景共享内存池）
     * @param systems  系统管理器（每场景独立的 System 集合）
     * @param nclPtrs  NCL 引擎指针（GameWorld*, PhysicsSystem*）
     */
    virtual void OnEnter(ECS::Registry&          registry,
                         ECS::SystemManager&     systems,
                         const Res_NCL_Pointers& nclPtrs) = 0;

    /**
     * @brief 场景卸载阶段（由 SceneManager 在切换前帧末调用）
     *
     * 应在此处：
     *   - 调用 systems.DestroyAll(registry)（逆序停机）
     *   - TODO: 调用 registry.Clear() 回收所有实体（待 Registry 实现后启用）
     *   - 释放场景独占资源（导航网格、特定纹理等）
     *
     * @param registry ECS 注册表
     * @param systems  系统管理器
     */
    virtual void OnExit(ECS::Registry&      registry,
                        ECS::SystemManager& systems) = 0;

    // ── 场景切换 ─────────────────────────────────────────────────
    IScene* GetNextScene() const { return m_NextScene; }
    void    ClearNextScene()    { m_NextScene = nullptr; }

    // ── 场景重启 ─────────────────────────────────────────────────
    /// 子类 override，返回同类型新场景实例（用于重启）
    virtual IScene* CreateRestartScene() { return nullptr; }

    /// 请求重启当前场景（延迟到帧末由 SceneManager 安全执行）
    void Restart() {
        if (IScene* s = CreateRestartScene()) RequestSceneChange(s);
    }

protected:
    /**
     * @brief 请求在下一帧末切换到指定场景（延迟切换机制）
     *
     * 严禁在 OnUpdate 期间直接 delete 当前场景，
     * 必须通过此接口写入 m_NextScene，由 SceneManager 在帧末安全执行切换。
     */
    void RequestSceneChange(IScene* next) { m_NextScene = next; }

    IScene* m_NextScene = nullptr; ///< 待切换目标（nullptr = 不切换）
};
