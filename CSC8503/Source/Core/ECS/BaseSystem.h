/**
 * @file BaseSystem.h
 * @brief ECS System 基类接口定义
 *
 * @details
 * 本文件定义了 ECS 框架中所有 System 必须实现的抽象基类 `ISystem`。
 *
 * ## System 的职责
 *
 * System 是 ECS 中唯一包含**逻辑**的层次。它不存储数据——数据全部存于
 * `Registry` 的组件池中。System 通过 `Registry::view<Ts...>()` 迭代
 * 自己感兴趣的实体子集，对其组件执行读写操作。
 *
 * ## 生命周期回调
 *
 * | 方法 | 调用时机 | 用途 |
 * |------|----------|------|
 * | `OnAwake(registry)` | 场景加载完成后，第一帧 Update 之前 | 初始化、订阅事件、预分配资源 |
 * | `OnUpdate(registry, dt)` | 每渲染帧，变步长 | 游戏逻辑、AI、输入响应 |
 * | `OnFixedUpdate(registry, fixedDt)` | 每物理帧，定步长（默认 1/60s） | 物理模拟、碰撞处理 |
 * | `OnDestroy(registry)` | 场景卸载或 SystemManager 清理时 | 释放 Jolt 物体、取消订阅、清理外部资源 |
 *
 * ## 执行顺序
 *
 * `SystemManager` 按注册时指定的**优先级（priority）**对系统排序（值越小越先执行）。
 * 推荐优先级约定：
 *
 * ```
 * 0   - 输入适配（InputAdapter）
 * 100 - 物理（Sys_Physics）
 * 200 - AI（Sys_AI）
 * 300 - 游戏逻辑（Sys_Stealth, Sys_Dialog ...）
 * 400 - 渲染桥接（Sys_Render, Sys_Camera）
 * 500 - UI（Sys_UI）
 * ```
 *
 * ## 如何实现一个 System
 *
 * @code
 * class Sys_Movement : public ECS::ISystem {
 * public:
 *     void OnUpdate(ECS::Registry& reg, float dt) override {
 *         reg.view<C_D_Transform, C_D_RigidBody>().each(
 *             [dt](ECS::EntityID, C_D_Transform& tf, C_D_RigidBody& rb) {
 *                 tf.position += rb.velocity * dt;
 *             }
 *         );
 *     }
 * };
 * @endcode
 *
 * @see Registry.h
 * @see SystemManager.h
 */

#pragma once

#include "Registry.h"

namespace ECS {

/**
 * @brief 所有 ECS System 的抽象基类，定义四个可覆盖的生命周期回调。
 *
 * @details
 * 所有虚函数均提供空默认实现，派生类只需覆盖所关心的回调。
 * System 通过 `SystemManager::Register<T>(priority)` 注册，
 * 并由 SystemManager 在主循环中批量调用。
 */
class ISystem {
public:
    virtual ~ISystem() = default;

    /**
     * @brief 场景加载后、首帧 Update 之前调用，用于一次性初始化。
     * @details 适合订阅 EventBus、预创建 Jolt 刚体、分配 GPU 资源等操作。
     * @param registry 当前场景的实体注册表。
     */
    virtual void OnAwake(Registry& registry) {}

    /**
     * @brief 每渲染帧调用，步长随帧率变化（Variable timestep）。
     * @details 适合处理输入响应、动画插值、相机跟随、UI 更新等逻辑。
     * @param registry 当前场景的实体注册表。
     * @param dt       距上一帧的时间间隔（秒）。
     */
    virtual void OnUpdate(Registry& registry, float dt) {}

    /**
     * @brief 每物理帧调用，步长固定（Fixed timestep，默认 1/60 秒）。
     * @details 适合驱动 Jolt 物理步进、确定性网络同步、碰撞事件分发等。
     * @param registry  当前场景的实体注册表。
     * @param fixedDt   固定物理帧步长（秒）。
     */
    virtual void OnFixedUpdate(Registry& registry, float fixedDt) {}

    /**
     * @brief 渲染完成后调用，用于后处理等需要渲染结果的操作。
     * @details 在 renderer->RenderScene() 之后、ImGui 渲染之前执行。
     *          适合后处理管线（Bloom、Tone Mapping、SSAO 等）。
     * @param registry 当前场景的实体注册表。
     * @param dt       距上一帧的时间间隔（秒）。
     */
    virtual void OnLateUpdate(Registry& registry, float dt) {}

    /**
     * @brief 场景卸载或 SystemManager 清理时调用，用于释放外部资源。
     * @details 适合销毁 Jolt 物理体、取消订阅 EventBus、释放音频句柄等。
     * @param registry 当前场景的实体注册表。
     */
    virtual void OnDestroy(Registry& registry) {}
};

} // namespace ECS
