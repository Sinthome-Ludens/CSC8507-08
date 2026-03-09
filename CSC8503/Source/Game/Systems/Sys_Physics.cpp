/**
 * @file Sys_Physics.cpp
 * @brief Jolt Physics ECS 物理系统实现。
 *
 * @details
 * 实现 Sys_Physics 的完整生命周期：Jolt 全局初始化、Body 创建/销毁、
 * Transform 双向同步、碰撞/触发事件发布，以及射线检测等工具函数。
 *
 * 固定步长驱动说明：
 * - `OnFixedUpdate` 执行单次 Jolt 步进（fixedDt = 1/60s），
 *   由 SceneManager 外部累加器保证每秒调用 60 次。
 * - `OnUpdate` 仅负责 Body 的创建/清理/参数同步，不执行步进。
 */
#include "Sys_Physics.h"
#include "Game/Utils/Assert.h"
#include "Game/Utils/Log.h"
#include <iostream>
#include <cfloat>
#include <cmath>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>

using namespace NCL::Maths;

// ============================================================
// Jolt ↔ NCL 转换工具（内部静态函数）
// ============================================================
JPH::Vec3 ECS::Sys_Physics::ToJolt(float x, float y, float z) {
    return JPH::Vec3(x, y, z);
}
JPH::Quat ECS::Sys_Physics::ToJoltQuat(float qx, float qy, float qz, float qw) {
    return JPH::Quat(qx, qy, qz, qw);
}

static Vector3 FromJolt(JPH::Vec3Arg v) {
    return Vector3(v.GetX(), v.GetY(), v.GetZ());
}
static Quaternion FromJoltQuat(JPH::QuatArg q) {
    return Quaternion(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

// ============================================================
// Jolt 全局初始化（只执行一次）
// ============================================================
namespace {
    bool g_JoltInitialized = false;
    std::unordered_map<uint32_t, float> g_LastGravityFactors;
}

/**
 * @brief 全局 Jolt 初始化（进程级单例，只执行一次）。
 * @details
 * 设置 Jolt 内部 Trace 和 AssertFailed 回调（必须在任何 Jolt API 调用之前完成），
 * 然后调用 RegisterDefaultAllocator / Factory::sInstance / RegisterTypes。
 * 由 g_JoltInitialized 标志保护，重复调用无副作用。
 */
void ECS::Sys_Physics::InitJolt() {
    if (g_JoltInitialized) return;

    JPH::Trace = [](const char* inFmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, inFmt);
        vsnprintf(buf, sizeof(buf), inFmt, args);
        va_end(args);
        LOG_INFO("[Jolt] " << buf);
    };

#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = [](const char* inExpression, const char* inMessage,
                           const char* inFile, JPH::uint inLine) -> bool {
        LOG_ERROR("[Jolt Assert] " << inFile << ":" << inLine
                  << " (" << inExpression << ") "
                  << (inMessage ? inMessage : ""));
        return true;
    };
#endif

    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    g_JoltInitialized = true;
    LOG_INFO("[Sys_Physics] Jolt global types registered");
}

// ============================================================
// OnAwake
// ============================================================

/**
 * @brief 场景加载后初始化 Jolt，创建 PhysicsSystem，注册自身指针到 ctx。
 * @details
 * 执行顺序：
 * 1. 调用 InitJolt()（全局单例，幂等）。
 * 2. 创建 10 MB TempAllocatorImpl 供物理步进使用。
 * 3. 创建 JobSystemThreadPool（hardware_concurrency - 1 工作线程）。
 * 4. 创建并初始化 PhysicsSystem（MAX_BODIES / MAX_PAIRS / MAX_CONTACTS）。
 * 5. 注册 ContactListener 到 PhysicsSystem。
 * 6. 将 Sys_Physics* 以无条件覆盖方式注入 registry ctx，
 *    防止场景切换后残留悬空指针。
 *    注意：EventBus 由 SceneManager::EnterScene() 在 OnAwake 之前注入 ctx，此处无需处理。
 * @param registry 当前场景注册表
 */
void ECS::Sys_Physics::OnAwake(Registry& registry) {
    InitJolt();

    m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    int numThreads = std::max(1, (int)std::thread::hardware_concurrency() - 1);
    m_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads);

    m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_PhysicsSystem->Init(
        MAX_BODIES, 0, MAX_PAIRS, MAX_CONTACTS,
        m_BPLayerInterface,
        m_ObjectVsBPFilter,
        m_ObjectLayerPairFilter);

    m_PhysicsSystem->SetContactListener(&m_ContactListener);

    registry.ctx_emplace<Sys_Physics*>(this);

    LOG_INFO("[Sys_Physics] OnAwake - Jolt PhysicsSystem initialized");
}

// ============================================================
// OnUpdate（Body 管理 + 参数同步，不执行步进）
// ============================================================

/**
 * @brief 每渲染帧调用：创建新 Body、清理孤立 Body、同步 gravity_factor。
 * @details
 * 执行顺序：
 * 1. 遍历所有拥有 C_D_Transform + C_D_RigidBody + C_D_Collider 的实体，
 *    对尚未创建 Jolt Body 的实体调用 CreateBodyForEntity()。
 * 2. 首帧所有静态体添加完毕后，调用一次 OptimizeBroadPhase() 提升碰撞查询性能。
 * 3. 调用 DestroyOrphanBodies() 清理已销毁实体的孤立 Body。
 * 4. 同步所有动态体的 gravity_factor 到 Jolt（支持运行时修改）；
 *    若重力从零恢复，主动唤醒对应 Body 防止 sleep 状态下力无效。
 * 步进、Transform 同步、碰撞事件发布均由 OnFixedUpdate 负责。
 * @param registry 当前场景注册表
 * @param dt       本帧变步长时间（秒，仅用于守卫逻辑，不传入 Jolt）
 */
void ECS::Sys_Physics::OnUpdate(Registry& registry, float dt) {
    if (!m_PhysicsSystem) return;

    registry.view<C_D_Transform, C_D_RigidBody, C_D_Collider>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col) {
            if (!rb.body_created) {
                CreateBodyForEntity(registry, id, tf, rb, col);
            }
        }
    );

    if (!m_BroadPhaseOptimized) {
        m_PhysicsSystem->OptimizeBroadPhase();
        m_BroadPhaseOptimized = true;
        LOG_INFO("[Sys_Physics] OptimizeBroadPhase called");
    }

    DestroyOrphanBodies(registry);

    {
        auto& bi = m_PhysicsSystem->GetBodyInterface();
        registry.view<C_D_RigidBody>().each([&](EntityID id, C_D_RigidBody& rb) {
            if (!rb.body_created || rb.is_static) return;

            const uint32_t bodyID = rb.jolt_body_id;
            const auto itPrev = g_LastGravityFactors.find(bodyID);
            const float previous = (itPrev != g_LastGravityFactors.end())
                ? itPrev->second
                : rb.gravity_factor;

            bi.SetGravityFactor(JPH::BodyID(rb.jolt_body_id), rb.gravity_factor);

            constexpr float EPS = 1e-4f;
            if (previous <= EPS && rb.gravity_factor > EPS) {
                bi.ActivateBody(JPH::BodyID(rb.jolt_body_id));
            }

            g_LastGravityFactors[bodyID] = rb.gravity_factor;
        });
    }
}

// ============================================================
// OnFixedUpdate（单次 Jolt 步进 + Transform 同步 + 事件发布）
// ============================================================

/**
 * @brief 固定步长物理更新入口：执行一次 Jolt 步进并回写 ECS 状态。
 * @details
 * 假设与前置条件：
 * - Sys_Physics 已完成 OnAwake 初始化（PhysicsSystem、TempAllocator、JobSystem 有效）。
 * - EventBus* 已由 SceneManager 在 EnterScene 时注入 registry ctx；
 *   FlushCollisionEvents 内部含 GAME_ASSERT 保护。
 * - @p fixedDt 由 SceneManager 的累加器提供（来源：Res_Time::fixedDeltaTime），
 *   本函数只执行一次步进，不在内部做时间累加或追帧逻辑。
 *
 * 副作用：
 * - 调用 Jolt::PhysicsSystem::Update 执行一次模拟步进。
 * - 将模拟后位姿同步回所有带 C_D_RigidBody 实体的 C_D_Transform。
 * - 通过 FlushCollisionEvents 将碰撞/触发信息发布到 EventBus。
 * @param registry 当前场景注册表
 * @param fixedDt  固定物理帧步长（秒），应等于 Res_Time::fixedDeltaTime
 */
void ECS::Sys_Physics::OnFixedUpdate(Registry& registry, float fixedDt) {
    if (!m_PhysicsSystem) return;

    m_PhysicsSystem->Update(fixedDt, 1, m_TempAllocator.get(), m_JobSystem.get());

    SyncTransformsFromJolt(registry, fixedDt);

    FlushCollisionEvents(registry);
}

// ============================================================
// OnDestroy
// ============================================================

/**
 * @brief 场景卸载时销毁所有 Jolt Body，释放 Jolt 资源，清除 ctx 中的裸指针。
 * @details
 * 执行顺序：
 * 1. 遍历 m_BodyToEntity，逐一调用 RemoveBody / DestroyBody，清空映射表。
 * 2. 按顺序析构：PhysicsSystem → JobSystem → TempAllocator（顺序不可颠倒）。
 * 3. 从 registry ctx 移除 Sys_Physics* 防止场景切换后悬空引用。
 *    注意：EventBus 由 SceneManager::ExitCurrentScene() 在 OnDestroy 之后清理。
 * 4. Jolt 全局资源（Factory 等）保持存活，避免多系统场景问题。
 * @param registry 当前场景注册表
 */
void ECS::Sys_Physics::OnDestroy(Registry& registry) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    for (auto& [bodyID, entityID] : m_BodyToEntity) {
        JPH::BodyID jid(bodyID);
        bi.RemoveBody(jid);
        bi.DestroyBody(jid);
    }
    m_BodyToEntity.clear();
    g_LastGravityFactors.clear();

    m_PhysicsSystem.reset();
    m_JobSystem.reset();
    m_TempAllocator.reset();

    if (registry.has_ctx<Sys_Physics*>()) {
        registry.ctx_erase<Sys_Physics*>();
    }

    m_BroadPhaseOptimized = false;
    LOG_INFO("[Sys_Physics] OnDestroy - Jolt PhysicsSystem destroyed");
}

// ============================================================
// CreateBodyForEntity
// ============================================================

/**
 * @brief 为指定 ECS 实体创建对应的 Jolt Body 并加入物理世界。
 * @details
 * 执行顺序：
 * 1. 根据 C_D_Collider::type（Box / Sphere / Capsule）构建 Jolt ShapeSettings。
 * 2. 根据 is_static / is_kinematic / is_trigger 确定 EMotionType 和 ObjectLayer：
 *    - static 或 trigger → Static / NON_MOVING
 *    - kinematic → Kinematic / MOVING
 *    - 其余 → Dynamic / MOVING
 * 3. 填充 BodyCreationSettings（质量/阻尼/摩擦/弹性/旋转锁定/Sensor 模式）。
 * 4. 将 EntityID 写入 Body::UserData 供碰撞回调反查。
 * 5. CreateBody + AddBody；Static Body 使用 DontActivate，其余使用 Activate。
 * 6. 将 jolt_body_id 回写至 rb，并在 m_BodyToEntity 中记录映射。
 * @param reg 当前场景注册表
 * @param id  目标实体 ID
 * @param tf  实体的 Transform 组件（提供初始位置/旋转）
 * @param rb  实体的 RigidBody 组件（接收 jolt_body_id 回写）
 * @param col 实体的 Collider 组件（提供形状/摩擦/触发器参数）
 */
void ECS::Sys_Physics::CreateBodyForEntity(
    Registry& reg, EntityID id,
    C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col)
{
    auto& bi = m_PhysicsSystem->GetBodyInterface();

    JPH::ShapeSettings::ShapeResult shapeResult;
    switch (col.type) {
        case ColliderType::Box: {
            shapeResult = JPH::BoxShapeSettings(ToJolt(col.half_x, col.half_y, col.half_z)).Create();
            break;
        }
        case ColliderType::Sphere: {
            shapeResult = JPH::SphereShapeSettings(col.half_x).Create();
            break;
        }
        case ColliderType::Capsule: {
            shapeResult = JPH::CapsuleShapeSettings(col.half_y, col.half_x).Create();
            break;
        }
    }

    if (shapeResult.HasError()) {
        LOG_ERROR("[Sys_Physics] Shape creation failed: " << shapeResult.GetError().c_str());
        return;
    }

    JPH::EMotionType motionType;
    JPH::ObjectLayer layer;
    if (rb.is_static || col.is_trigger) {
        motionType = JPH::EMotionType::Static;
        layer      = PhysicsLayers::NON_MOVING;
    } else if (rb.is_kinematic) {
        motionType = JPH::EMotionType::Kinematic;
        layer      = PhysicsLayers::MOVING;
    } else {
        motionType = JPH::EMotionType::Dynamic;
        layer      = PhysicsLayers::MOVING;
    }

    JPH::BodyCreationSettings bcs(
        shapeResult.Get(),
        JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
        JPH::Quat(tf.rotation.x, tf.rotation.y, tf.rotation.z, tf.rotation.w),
        motionType,
        layer
    );

    if (motionType == JPH::EMotionType::Dynamic) {
        bcs.mMassPropertiesOverride.mMass = rb.mass;
        bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bcs.mLinearDamping  = rb.linear_damping;
        bcs.mAngularDamping = rb.angular_damping;
        bcs.mGravityFactor  = rb.gravity_factor;
    }

    bcs.mFriction    = col.friction;
    bcs.mRestitution = col.restitution;
    bcs.mIsSensor    = col.is_trigger;

    if (rb.lock_rotation_x || rb.lock_rotation_y || rb.lock_rotation_z) {
        JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::All;
        if (rb.lock_rotation_x) dofs = dofs & ~JPH::EAllowedDOFs::RotationX;
        if (rb.lock_rotation_y) dofs = dofs & ~JPH::EAllowedDOFs::RotationY;
        if (rb.lock_rotation_z) dofs = dofs & ~JPH::EAllowedDOFs::RotationZ;
        bcs.mAllowedDOFs = dofs;
    }

    bcs.mUserData = (uint64_t)id;

    JPH::Body* body = bi.CreateBody(bcs);
    if (!body) {
        LOG_ERROR("[Sys_Physics] Failed to create Jolt Body for entity " << id);
        return;
    }

    bi.AddBody(body->GetID(),
               (motionType == JPH::EMotionType::Static)
                   ? JPH::EActivation::DontActivate
                   : JPH::EActivation::Activate);

    uint32_t rawID = body->GetID().GetIndexAndSequenceNumber();
    rb.jolt_body_id  = rawID;
    rb.body_created  = true;
    m_BodyToEntity[rawID] = id;
}

// ============================================================
// SyncTransformsFromJolt（动态体 Jolt→ECS / 运动学体 ECS→Jolt）
// ============================================================

/**
 * @brief 在每次 Jolt 步进后双向同步 Transform。
 * @details
 * - 运动学体（is_kinematic）：将 ECS C_D_Transform（由输入/插值驱动）写回 Jolt，
 *   使其在物理世界中实际移动，从而能与动态体产生碰撞反应。
 *   MoveKinematic 使用传入的 fixedDt 保证速度计算与模拟步长一致。
 * - 动态体：从 Jolt 读取积分后的位置/旋转，写回 C_D_Transform，驱动渲染。
 *   处于 sleep 或未加入世界的 Body 跳过。
 * - 静态体（is_static）不参与同步。
 * @param reg     当前场景注册表
 * @param fixedDt 固定物理帧步长（秒），用于运动学体 MoveKinematic 的速度计算
 */
void ECS::Sys_Physics::SyncTransformsFromJolt(Registry& reg, float fixedDt) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();

    reg.view<C_D_Transform, C_D_RigidBody>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb) {
            if (!rb.body_created || rb.is_static) return;

            JPH::BodyID jid(rb.jolt_body_id);

            if (rb.is_kinematic) {
                bi.MoveKinematic(
                    jid,
                    JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
                    JPH::Quat(tf.rotation.x, tf.rotation.y, tf.rotation.z, tf.rotation.w),
                    fixedDt);
                return;
            }

            if (!bi.IsActive(jid) && !bi.IsAdded(jid)) return;

            JPH::RVec3 pos = bi.GetPosition(jid);
            JPH::Quat  rot = bi.GetRotation(jid);

            tf.position = Vector3(pos.GetX(), pos.GetY(), pos.GetZ());
            tf.rotation = Quaternion(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW());
        }
    );
}

// ============================================================
// FlushCollisionEvents
// ============================================================

/**
 * @brief 将本物理步产生的碰撞/触发事件发布到 EventBus。
 * @details
 * 从 ECSContactListener 的 pending 队列（线程安全 swap）取出本步所有接触事件。
 * 触发判断以 C_D_Collider::is_trigger 为准（非 PendingContact::is_trigger），
 * 确保 Exit 事件与 Enter 事件使用相同的验证路径：
 * - 若 entA 或 entB 的 Collider 标记为 trigger，发布 Evt_Phys_TriggerEnter/Exit，
 *   entity_trigger 为拥有 is_trigger=true 的一方。
 * - 否则发布普通 Evt_Phys_Collision（Exit 事件不发布碰撞事件）。
 * 前置条件：EventBus* 必须已注入 registry ctx（GAME_ASSERT 保护）。
 * @param reg 当前场景注册表
 */
void ECS::Sys_Physics::FlushCollisionEvents(Registry& reg) {
    std::vector<ECSContactListener::PendingContact> contacts;
    {
        std::lock_guard lock(m_ContactListener.mutex);
        contacts.swap(m_ContactListener.pending);
    }
    if (contacts.empty()) return;

    GAME_ASSERT(reg.has_ctx<ECS::EventBus*>() && reg.ctx<ECS::EventBus*>() != nullptr,
                "[Sys_Physics] FlushCollisionEvents: EventBus* not found in registry ctx. "
                "SceneManager must inject EventBus before OnFixedUpdate is called.");
    auto& bus = *reg.ctx<ECS::EventBus*>();

    for (auto& c : contacts) {
        auto itA = m_BodyToEntity.find(c.body_id_a);
        auto itB = m_BodyToEntity.find(c.body_id_b);
        if (itA == m_BodyToEntity.end() || itB == m_BodyToEntity.end()) continue;

        EntityID entA = itA->second;
        EntityID entB = itB->second;

        const bool hasColliderA = reg.Has<C_D_Collider>(entA);
        const bool hasColliderB = reg.Has<C_D_Collider>(entB);
        const bool isTriggerA = hasColliderA && reg.Get<C_D_Collider>(entA).is_trigger;
        const bool isTriggerB = hasColliderB && reg.Get<C_D_Collider>(entB).is_trigger;

        if (isTriggerA || isTriggerB) {
            const EntityID entityTrigger = isTriggerA ? entA : entB;
            const EntityID entityOther   = isTriggerA ? entB : entA;

            if (c.is_exit) {
                Evt_Phys_TriggerExit evt;
                evt.entity_trigger = entityTrigger;
                evt.entity_other   = entityOther;
                bus.publish<Evt_Phys_TriggerExit>(evt);
            } else {
                Evt_Phys_TriggerEnter evt;
                evt.entity_trigger = entityTrigger;
                evt.entity_other   = entityOther;
                bus.publish<Evt_Phys_TriggerEnter>(evt);
            }
            continue;
        }

        if (c.is_exit) continue;

        Evt_Phys_Collision evt;
        evt.entity_a            = entA;
        evt.entity_b            = entB;
        evt.contact_point       = Vector3(c.contact_x, c.contact_y, c.contact_z);
        evt.contact_normal      = Vector3(c.normal_x,  c.normal_y,  c.normal_z);
        evt.separating_velocity = c.separating_velocity;
        bus.publish<Evt_Phys_Collision>(evt);
    }
}

// ============================================================
// DestroyOrphanBodies（清理已销毁实体的 Jolt Body）
// ============================================================

/**
 * @brief 清理已销毁 ECS 实体的孤立 Jolt Body。
 * @details
 * 遍历 m_BodyToEntity，找出对应实体已无效（registry.Valid() == false）的 Body。
 * 移除每个孤立 Body 前，先在其包围盒扩展范围内激活所有睡眠的动态体：
 * Jolt 的 RemoveBody 不会自动唤醒邻近体，否则叠放在上方的物体因 sleep
 * 状态永远不更新位置，视觉上会悬浮在空中。包围盒向上扩展 2m 以覆盖紧贴上方的实体。
 * @param reg 当前场景注册表
 */
void ECS::Sys_Physics::DestroyOrphanBodies(Registry& reg) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    std::vector<uint32_t> toRemove;

    for (auto& [bodyID, entityID] : m_BodyToEntity) {
        if (!reg.Valid(entityID)) {
            toRemove.push_back(bodyID);
        }
    }

    for (uint32_t rawID : toRemove) {
        JPH::BodyID jid(rawID);

        if (bi.IsAdded(jid)) {
            JPH::AABox bounds = bi.GetTransformedShape(jid).GetWorldSpaceBounds();
            bounds.mMax += JPH::Vec3(0.1f, 2.0f, 0.1f);

            struct AllBPFilter : public JPH::BroadPhaseLayerFilter {
                bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
            };
            struct MovingObjFilter : public JPH::ObjectLayerFilter {
                bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
                    return inLayer == PhysicsLayers::MOVING;
                }
            };
            bi.ActivateBodiesInAABox(bounds, AllBPFilter{}, MovingObjFilter{});
        }

        bi.RemoveBody(jid);
        bi.DestroyBody(jid);
        m_BodyToEntity.erase(rawID);
    }
}

// ============================================================
// 工具函数
// ============================================================
void ECS::Sys_Physics::SetLinearVelocity(uint32_t joltBodyID, float vx, float vy, float vz) {
    if (!m_PhysicsSystem) return;
    m_PhysicsSystem->GetBodyInterface().SetLinearVelocity(
        JPH::BodyID(joltBodyID), ToJolt(vx, vy, vz));
}

void ECS::Sys_Physics::ApplyImpulse(uint32_t joltBodyID, float ix, float iy, float iz) {
    if (!m_PhysicsSystem) return;
    m_PhysicsSystem->GetBodyInterface().AddImpulse(
        JPH::BodyID(joltBodyID), ToJolt(ix, iy, iz));
}

void ECS::Sys_Physics::AddForce(uint32_t joltBodyID, float fx, float fy, float fz) {
    if (!m_PhysicsSystem) return;
    m_PhysicsSystem->GetBodyInterface().AddForce(
        JPH::BodyID(joltBodyID), ToJolt(fx, fy, fz));
}

Vector3 ECS::Sys_Physics::GetLinearVelocity(uint32_t joltBodyID) {
    if (!m_PhysicsSystem) return Vector3(0, 0, 0);
    JPH::Vec3 v = m_PhysicsSystem->GetBodyInterface().GetLinearVelocity(
        JPH::BodyID(joltBodyID));
    return Vector3(v.GetX(), v.GetY(), v.GetZ());
}

void ECS::Sys_Physics::MoveKinematic(
    uint32_t joltBodyID,
    float px, float py, float pz,
    float qx, float qy, float qz, float qw,
    float dt)
{
    if (!m_PhysicsSystem) return;
    m_PhysicsSystem->GetBodyInterface().MoveKinematic(
        JPH::BodyID(joltBodyID),
        JPH::RVec3(px, py, pz),
        JPH::Quat(qx, qy, qz, qw),
        dt);
}

// ============================================================
// SetRotation（供 Sys_Navigation 调用 - 来自 feat/navmesh-system）
// ============================================================
void ECS::Sys_Physics::SetRotation(uint32_t joltBodyID,
                                   const NCL::Maths::Quaternion& rotation)
{
    if (!m_PhysicsSystem) return;
    m_PhysicsSystem->GetBodyInterface().SetRotation(
        JPH::BodyID(joltBodyID),
        ToJoltQuat(rotation.x, rotation.y, rotation.z, rotation.w),
        JPH::EActivation::Activate);
}

// ============================================================
// CastRay — 射线检测（POD 接口 - 来自 master）
// ============================================================

/**
 * @brief 从指定原点沿方向发射射线，返回第一个命中结果。
 * @details
 * 方向向量（dx,dy,dz）内部归一化，调用方无需提前归一化。
 * 零方向向量（长度 < 1e-6）直接返回未命中。
 * 构造 Jolt RRayCast 时将方向乘以 maxDist，使 fraction=1.0 对应最大距离处。
 * 命中时通过 BodyLockRead 获取命中面的世界空间法线。
 * 对外只返回 ECS EntityID，不暴露 Jolt BodyID。
 * @param ox,oy,oz 射线原点
 * @param dx,dy,dz 射线方向（无需归一化）
 * @param maxDist  最大检测距离
 * @return RaycastHit 命中信息（hit=false 表示未命中）
 */
ECS::Sys_Physics::RaycastHit ECS::Sys_Physics::CastRay(
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    float maxDist)
{
    RaycastHit result{};
    if (!m_PhysicsSystem) return result;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-6f) return result;
    dx /= len;
    dy /= len;
    dz /= len;
    JPH::RRayCast ray(JPH::RVec3(ox, oy, oz),
                      JPH::Vec3(dx * maxDist, dy * maxDist, dz * maxDist));
    JPH::RayCastResult hit;
    hit.mFraction = 1.0f + FLT_EPSILON;
    const auto& query = m_PhysicsSystem->GetNarrowPhaseQuery();
    if (query.CastRay(ray, hit)) {
        result.hit      = true;
        result.fraction = hit.mFraction;
        const uint32_t rawBodyID = hit.mBodyID.GetIndexAndSequenceNumber();
        const auto itEntity = m_BodyToEntity.find(rawBodyID);
        if (itEntity != m_BodyToEntity.end()) {
            result.entity = itEntity->second;
        }
        JPH::RVec3 hitPoint = ray.GetPointOnRay(hit.mFraction);
        result.pointX = static_cast<float>(hitPoint.GetX());
        result.pointY = static_cast<float>(hitPoint.GetY());
        result.pointZ = static_cast<float>(hitPoint.GetZ());
        JPH::BodyLockRead lock(m_PhysicsSystem->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 normal = body.GetWorldSpaceSurfaceNormal(
                hit.mSubShapeID2, hitPoint);
            result.normalX = normal.GetX();
            result.normalY = normal.GetY();
            result.normalZ = normal.GetZ();
        }
    }
    return result;
}

// ============================================================
// ReplaceShapeCapsule — 运行时替换碰撞体为 Capsule (来自 master)
// ============================================================

/**
 * @brief 运行时将指定 Body 的碰撞体形状替换为 Capsule（姿态切换用）。
 * @details
 * 创建新的 CapsuleShapeSettings 并通过 BodyInterface::SetShape 替换。
 * 传入 false（不用新形状默认密度重算质量），保留原有质量属性。
 * 替换后 Body 被激活（EActivation::Activate）。
 * @param joltBodyID 目标 Jolt Body 的原始 ID
 * @param halfHeight Capsule 半高（不含球帽）
 * @param radius     Capsule 半径
 */
void ECS::Sys_Physics::ReplaceShapeCapsule(uint32_t joltBodyID, float halfHeight, float radius) {
    if (!m_PhysicsSystem) return;
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    JPH::BodyID jid(joltBodyID);
    JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
    auto shapeResult = capsuleSettings.Create();
    if (shapeResult.HasError()) {
        LOG_ERROR("[Sys_Physics] ReplaceShapeCapsule failed: " << shapeResult.GetError().c_str());
        return;
    }
    bi.SetShape(jid, shapeResult.Get(), false,
                JPH::EActivation::Activate);
}

// ============================================================
// SetPosition — 直接设置动态体世界位置 (来自 master)
// ============================================================
void ECS::Sys_Physics::SetPosition(uint32_t joltBodyID, float px, float py, float pz) {
    if (!m_PhysicsSystem) return;
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    bi.SetPosition(JPH::BodyID(joltBodyID),
                   JPH::RVec3(px, py, pz),
                   JPH::EActivation::Activate);
}

// ============================================================
// ActivateBody — 强制唤醒 Body (来自 master)
// ============================================================
void ECS::Sys_Physics::ActivateBody(uint32_t joltBodyID) {
    if (!m_PhysicsSystem) return;
    m_PhysicsSystem->GetBodyInterface().ActivateBody(JPH::BodyID(joltBodyID));
}
