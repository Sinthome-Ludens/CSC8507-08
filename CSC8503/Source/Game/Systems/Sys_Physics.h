/**
 * @file Sys_Physics.h
 * @brief Jolt Physics ECS 物理系统声明。
 *
 * @details
 * 定义碰撞层过滤器、ContactListener 以及核心系统类 `ECS::Sys_Physics`。
 *
 * 系统生命周期：
 * - `OnAwake`       : 初始化 Jolt，创建 PhysicsSystem，注册 Sys_Physics* 到 ctx
 * - `OnUpdate`      : 检测并创建新实体 Body、清理孤立 Body、同步 gravity_factor
 * - `OnFixedUpdate` : 单次 Jolt 步进（由 SceneManager 外部累加器驱动），
 *                     同步结果至 C_D_Transform，发布碰撞/触发事件
 * - `OnDestroy`     : 销毁所有 Jolt Body，释放 Sys_Physics* ctx
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
        /// Enter 事件：由 OnContactAdded 根据 IsSensor() 填充；
        /// Exit 事件（is_exit=true）：固定为 false，触发判断由
        /// FlushCollisionEvents 通过 C_D_Collider::is_trigger 重新验证。
        bool     is_trigger;
        bool     is_exit;  ///< true = TriggerExit
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
        // 只记录 body ID，是否为 trigger 由 Sys_Physics::FlushCollisionEvents 查表验证
        pending.push_back({
            pair.GetBody1ID().GetIndexAndSequenceNumber(),
            pair.GetBody2ID().GetIndexAndSequenceNumber(),
            0,0,0, 0,0,0, 0.0f,
            false, // 退出时无法直接判断，后续由 FlushCollisionEvents 验证
            true   // exit 事件
        });
    }
};

// ============================================================
// Sys_Physics — Jolt ECS 物理系统
// ============================================================
namespace ECS {

class Sys_Physics : public ISystem {
public:
    // 物理常量
    static constexpr float FIXED_DT    = 1.0f / 60.0f;  ///< 固定物理步长（60 Hz）
    static constexpr int   MAX_BODIES  = 1024;           ///< 最大刚体数
    static constexpr int   MAX_PAIRS   = 1024;           ///< 最大碰撞对数
    static constexpr int   MAX_CONTACTS= 1024;           ///< 最大接触约束数

    /**
     * @brief 场景加载后初始化 Jolt，创建 PhysicsSystem，并注册自身指针到 ctx。
     * @details EventBus 由 SceneManager 在进入场景前注入，此处不再负责其生命周期。
     * @param registry 当前场景注册表
     */
    void OnAwake(Registry& registry) override;

    /**
     * @brief 每渲染帧调用：检测并创建新实体 Body、清理孤立 Body、同步 gravity_factor。
     * @details 不执行 Jolt 步进，步进由 OnFixedUpdate 负责。
     * @param registry 当前场景注册表
     * @param dt       本帧变步长时间（秒）
     */
    void OnUpdate(Registry& registry, float dt) override;

    /**
     * @brief 每物理帧调用（固定步长 1/60s），由 SceneManager 外部累加器驱动。
     * @details 执行单次 Jolt 步进，随后将结果同步回 C_D_Transform，并发布碰撞/触发事件。
     * @param registry 当前场景注册表
     * @param fixedDt  固定物理帧步长（秒），应等于 FIXED_DT
     */
    void OnFixedUpdate(Registry& registry, float fixedDt) override;

    /**
     * @brief 场景卸载时销毁所有 Jolt Body，释放 Jolt 资源，清除 ctx 中的裸指针。
     * @param registry 当前场景注册表
     */
    void OnDestroy(Registry& registry) override;

    // --- 工具函数（供 Prefab 工厂等外部代码调用）---

    /// 在 ECS 实体上设置 Jolt 线速度（动态体）
    void SetLinearVelocity(uint32_t joltBodyID, float vx, float vy, float vz);

    /// 直接设置 Jolt 刚体的旋转（供 Sys_Navigation 调用）
    void SetRotation(uint32_t joltBodyID, const NCL::Maths::Quaternion& rotation);

    /// 给 Jolt 刚体施加冲量（动态体）
    void ApplyImpulse(uint32_t joltBodyID, float ix, float iy, float iz);

    /// 直接设置 Kinematic 体的目标位置
    void MoveKinematic(uint32_t joltBodyID,
                       float px, float py, float pz,
                       float qx, float qy, float qz, float qw,
                       float dt);

    /// 给 Jolt 刚体施加持续力（动态体，需每帧调用）
    void AddForce(uint32_t joltBodyID, float fx, float fy, float fz);

    /// 获取 Jolt 刚体当前线速度（动态体）
    NCL::Maths::Vector3 GetLinearVelocity(uint32_t joltBodyID);

    /// 获取 Jolt PhysicsSystem 指针（供调试/ImGui 使用）
    JPH::PhysicsSystem* GetJoltPhysicsSystem() const { return m_PhysicsSystem.get(); }

    // --- 射线检测 / 碰撞体替换 / 位置设置（供 Sys_Gameplay / Sys_Movement 等系统调用）---

    /// 射线检测结果（POD，不暴露 Jolt 类型）
    struct RaycastHit {
        bool     hit       = false;
        float    fraction  = 1.0f;
        float    pointX    = 0.0f, pointY = 0.0f, pointZ = 0.0f;
        float    normalX   = 0.0f, normalY = 0.0f, normalZ = 0.0f;
        EntityID entity    = Entity::NULL_ENTITY; ///< 命中的 ECS 实体 ID（统一对外语义）
    };

    /// 从 (ox,oy,oz) 沿 (dx,dy,dz) 方向射线检测，最大距离 maxDist
    /// 方向向量无需归一化，内部会自动归一化
    /// 对外只返回 ECS EntityID，不暴露 Jolt BodyID
    RaycastHit CastRay(float ox, float oy, float oz,
                       float dx, float dy, float dz,
                       float maxDist);

    /// 运行时替换指定 Body 的碰撞体形状为 Capsule（姿态切换用）
    void ReplaceShapeCapsule(uint32_t joltBodyID, float halfHeight, float radius);

    /// 直接设置动态体的世界位置（贴墙吸附用）
    void SetPosition(uint32_t joltBodyID, float px, float py, float pz);

    /// 强制激活 Body（防止 sleep 状态下 AddForce 无效）
    void ActivateBody(uint32_t joltBodyID);

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

    // BroadPhase 优化标志（场景加载完毕后调用一次 OptimizeBroadPhase）
    bool m_BroadPhaseOptimized = false;

    // --- 私有方法 ---
    void InitJolt();
    void CreateBodyForEntity(Registry& reg, EntityID id,
                             C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col);
    void SyncTransformsFromJolt(Registry& reg, float fixedDt);
    void FlushCollisionEvents(Registry& reg);
    void DestroyOrphanBodies(Registry& reg);
    // NCL ↔ Jolt 转换
    static JPH::Vec3  ToJolt(float x, float y, float z);
    static JPH::Quat  ToJoltQuat(float qx, float qy, float qz, float qw);
};

} // namespace ECS
