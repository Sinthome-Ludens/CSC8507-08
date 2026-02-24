/**
 * @file Sys_Physics.h
 * @brief ECS 物理系统接口与物理事件缓存结构定义。
 *
 * @details
 * 本文件定义基于 Jolt 的 ECS 物理系统 `ECS::Sys_Physics`，以及其依赖的
 * 层过滤器与接触监听器结构。
 *
 * ## 主要职责
 * - 维护 ECS 实体与 Jolt Body 的生命周期映射。
 * - 在固定步长路径执行物理推进与事件分发。
 * - 在变步长路径执行建体、清理、参数同步与运动学体推送。
 *
 * ## 公有方法作用
 * - `OnAwake`：初始化 Jolt 运行环境并准备系统资源。
 * - `OnUpdate`：执行变步长安全逻辑（不做物理积分）。
 * - `OnFixedUpdate`：执行固定步长物理推进与结果回写。
 * - `OnDestroy`：释放物理对象、清理映射和事件总线。
 * - `SetLinearVelocity`：设置动态体线速度。
 * - `ApplyImpulse`：向动态体施加冲量。
 * - `MoveKinematic`：设置运动学体目标位姿。
 * - `RaycastNearest`：执行单射线最近命中查询并输出统一命中结果。
 *
 * ## Raycast 过滤优先级（固定顺序）
 * 1. `ignore_entity`
 * 2. `include_triggers`
 * 3. `layer_mask`
 * 4. `tag_mask`
 *
 * ## 位掩码扩展约定
 * - 位定义遵循“只新增不重排”，已占用位不可改语义。
 * - `layer_mask` 用于粗粒度物理层筛选，`tag_mask` 用于细粒度语义筛选。
 */

#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Core/ECS/EventBus.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Events/Evt_Phys_Collision.h"
#include "Game/Events/Evt_Phys_Trigger.h"

// Jolt Physics
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/IssueReporting.h>
#include <cstdarg>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

// ============================================================
// 碰撞层定义
// ============================================================
namespace PhysicsLayers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0; ///< 静态体（地板、墙壁）
    static constexpr JPH::ObjectLayer MOVING     = 1; ///< 动态体（玩家、敌人、道具）
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace BPLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint            NUM_LAYERS(2);
}

// ============================================================
// BroadPhase 层接口
// ============================================================
class ECSBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    virtual JPH::uint GetNumBroadPhaseLayers() const override {
        return BPLayers::NUM_LAYERS;
    }
    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        switch (inLayer) {
            case PhysicsLayers::NON_MOVING: return BPLayers::NON_MOVING;
            case PhysicsLayers::MOVING:     return BPLayers::MOVING;
            default: JPH_ASSERT(false);     return BPLayers::NON_MOVING;
        }
    }
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BPLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BPLayers::MOVING:     return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
};

// Object 层 vs BroadPhase 层过滤器
class ECSObjectVsBPLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case PhysicsLayers::NON_MOVING: return inLayer2 == BPLayers::MOVING;
            case PhysicsLayers::MOVING:     return true;
            default: JPH_ASSERT(false);     return false;
        }
    }
};

// Object 层 vs Object 层过滤器
class ECSObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case PhysicsLayers::NON_MOVING: return inObject2 == PhysicsLayers::MOVING;
            case PhysicsLayers::MOVING:     return true;
            default: JPH_ASSERT(false);     return false;
        }
    }
};

// ============================================================
// Contact Listener（线程安全，收集事件到 Pending 队列）
// ============================================================
class ECSContactListener final : public JPH::ContactListener {
public:
    struct PendingContact {
        uint32_t body_id_a;
        uint32_t body_id_b;
        float    contact_x, contact_y, contact_z;
        float    normal_x,  normal_y,  normal_z;
        float    separating_velocity;
        bool     is_trigger; ///< Jolt 侧传感器标记（最终语义由 Sys_Physics 结合 ECS 组件复核）
        bool     is_exit;    ///< true = TriggerExit 候选（需在 FlushCollisionEvents 中再次校验）
    };

    std::mutex                   mutex;
    std::vector<PendingContact>  pending;

    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body&, const JPH::Body&,
        JPH::RVec3Arg, const JPH::CollideShapeResult&) override
    {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(
        const JPH::Body& body1, const JPH::Body& body2,
        const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) override
    {
        bool isTrigger1 = body1.IsSensor();
        bool isTrigger2 = body2.IsSensor();
        JPH::Vec3 cp = manifold.GetWorldSpaceContactPointOn1(0);
        JPH::Vec3 n  = manifold.mWorldSpaceNormal;

        std::lock_guard lock(mutex);
        pending.push_back({
            body1.GetID().GetIndexAndSequenceNumber(),
            body2.GetID().GetIndexAndSequenceNumber(),
            cp.GetX(), cp.GetY(), cp.GetZ(),
            n.GetX(),  n.GetY(),  n.GetZ(),
            0.0f,
            isTrigger1 || isTrigger2,
            false
        });
    }

    virtual void OnContactPersisted(
        const JPH::Body&, const JPH::Body&,
        const JPH::ContactManifold&, JPH::ContactSettings&) override {}

    virtual void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
        std::lock_guard lock(mutex);
        // 只记录 body ID，is_exit=true 由 Sys_Physics 在 FlushCollisionEvents 中二次判定
        pending.push_back({
            pair.GetBody1ID().GetIndexAndSequenceNumber(),
            pair.GetBody2ID().GetIndexAndSequenceNumber(),
            0,0,0, 0,0,0, 0.0f,
            true,  // 先标记为 trigger 候选，最终以 ECS::C_D_Collider::is_trigger 为准
            true   // exit 候选
        });
    }
};

// ============================================================
// Sys_Physics — Jolt ECS 物理系统
// ============================================================
namespace ECS {

/**
 * @brief 单射线查询过滤参数。
 *
 * @details
 * 过滤参数同时支持层与标签双轨筛选，默认值为全开掩码。
 */
struct RaycastFilter {
    uint32_t layer_mask      = 0xFFFFFFFFu;         ///< 层过滤位（粗粒度）
    uint32_t tag_mask        = 0xFFFFFFFFu;         ///< 标签过滤位（细粒度）
    bool     include_triggers = true;               ///< 是否允许命中 Trigger
    EntityID ignore_entity   = Entity::NULL_ENTITY; ///< 忽略实体（通常用于忽略自身）
};

/**
 * @brief 单射线最近命中结果。
 *
 * @details
 * 所有几何结果均为世界空间语义：`point` 为命中点，`normal` 为命中面法线，
 * `distance` 为从 ray 起点到命中点的世界距离。
 */
struct RaycastHit {
    bool     hit          = false;              ///< 是否命中
    EntityID entity       = Entity::NULL_ENTITY;///< 命中的 ECS 实体
    uint32_t jolt_body_id = 0xFFFFFFFFu;        ///< 命中的 Jolt BodyID 原始值
    Vector3  point        = Vector3();          ///< 命中点（世界坐标）
    Vector3  normal       = Vector3();          ///< 命中法线（世界坐标）
    float    distance     = 0.0f;               ///< 命中距离（世界距离）
};

class Sys_Physics : public ISystem {
public:
    // 物理常量
    static constexpr float FIXED_DT    = 1.0f / 60.0f;  ///< 固定物理步长（60 Hz）
    static constexpr int   MAX_BODIES  = 1024;           ///< 最大刚体数
    static constexpr int   MAX_PAIRS   = 1024;           ///< 最大碰撞对数
    static constexpr int   MAX_CONTACTS= 1024;           ///< 最大接触约束数

    void OnAwake  (Registry& registry) override;
    // 变步长职责：建体、清理、参数同步、运动学体 ECS->Jolt 推送（不做物理积分）
    void OnUpdate (Registry& registry, float dt) override;
    // 固定步长职责：Jolt 物理步进 + 动态体 Jolt->ECS 回写 + 碰撞/触发事件分发
    void OnFixedUpdate(Registry& registry, float fixedDt) override;
    void OnDestroy(Registry& registry) override;

    // --- 工具函数（供 Prefab 工厂等外部代码调用）---

    /// 在 ECS 实体上设置 Jolt 线速度（动态体）
    void SetLinearVelocity(uint32_t joltBodyID, float vx, float vy, float vz);

    /// 给 Jolt 刚体施加冲量（动态体）
    void ApplyImpulse(uint32_t joltBodyID, float ix, float iy, float iz);

    /// 直接设置 Kinematic 体的目标位置
    void MoveKinematic(uint32_t joltBodyID,
                       float px, float py, float pz,
                       float qx, float qy, float qz, float qw,
                       float dt);

    /// 执行单射线最近命中查询，按固定过滤顺序返回最近有效命中
    bool RaycastNearest(const Vector3& origin,
                        const Vector3& direction,
                        float maxDistance,
                        const RaycastFilter& filter,
                        RaycastHit& outHit) const;

    /// 获取 Jolt PhysicsSystem 指针（供调试/ImGui 使用）
    JPH::PhysicsSystem* GetJoltPhysicsSystem() const { return m_PhysicsSystem.get(); }

private:
    // --- Jolt 对象（生命周期由 Sys_Physics 管理）---
    std::unique_ptr<JPH::TempAllocatorImpl>   m_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;
    std::unique_ptr<JPH::PhysicsSystem>       m_PhysicsSystem;

    // Jolt 接口（必须在 PhysicsSystem 之前有效）
    ECSBroadPhaseLayerInterface m_BPLayerInterface;
    ECSObjectVsBPLayerFilter    m_ObjectVsBPFilter;
    ECSObjectLayerPairFilter    m_ObjectLayerPairFilter;
    ECSContactListener          m_ContactListener;

    // --- 映射表 ---
    // jolt_body_id (uint32) → EntityID，用于碰撞事件的实体查找
    std::unordered_map<uint32_t, EntityID> m_BodyToEntity;

    // EventBus 由 Sys_Physics 持有（EventBus 不可复制，无法直接存入 std::any）
    // 通过 registry.ctx_emplace<ECS::EventBus*>(ptr) 以裸指针注册到 Context
    std::unique_ptr<ECS::EventBus> m_EventBus;

    // BroadPhase 优化标志（场景加载完毕后调用一次 OptimizeBroadPhase）
    bool m_BroadPhaseOptimized = false;

    // --- 私有方法 ---
    void InitJolt();
    void CreateBodyForEntity(Registry& reg, EntityID id,
                             C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col);
    void SyncTransformsFromJolt(Registry& reg);
    void FlushCollisionEvents(Registry& reg);
    void DestroyOrphanBodies(Registry& reg);

    // NCL ↔ Jolt 转换
    static JPH::Vec3  ToJolt(float x, float y, float z);
    static JPH::Quat  ToJoltQuat(float qx, float qy, float qz, float qw);
};

} // namespace ECS
