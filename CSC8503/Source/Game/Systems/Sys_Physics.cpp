#include "Sys_Physics.h"
#include "Game/Utils/Log.h"
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <iostream>

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
}

void ECS::Sys_Physics::InitJolt() {
    if (g_JoltInitialized) return;

    // Jolt 内部 trace/assert 处理器（必须在任何 Jolt 调用之前设置）
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
        return true; // true = break into debugger
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
void ECS::Sys_Physics::OnAwake(Registry& registry) {
    InitJolt();

    // 10 MB 临时分配器（物理步进用）
    m_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    // 线程池作业系统（使用 hardware_concurrency - 1 个工作线程）
    int numThreads = std::max(1, (int)std::thread::hardware_concurrency() - 1);
    m_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads);

    // 创建 PhysicsSystem
    m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
    m_PhysicsSystem->Init(
        MAX_BODIES, 0, MAX_PAIRS, MAX_CONTACTS,
        m_BPLayerInterface,
        m_ObjectVsBPFilter,
        m_ObjectLayerPairFilter);

    // 注册 ContactListener
    m_PhysicsSystem->SetContactListener(&m_ContactListener);

    // 注册 EventBus 到 Registry Context（EventBus 不可复制，以裸指针注册）
    if (!registry.has_ctx<ECS::EventBus*>()) {
        m_EventBus = std::make_unique<ECS::EventBus>();
        registry.ctx_emplace<ECS::EventBus*>(m_EventBus.get());
    }

    LOG_INFO("[Sys_Physics] OnAwake - Jolt PhysicsSystem initialized");
}

// ============================================================
// OnUpdate（仅处理变步长安全逻辑，不做物理积分）
// ============================================================
void ECS::Sys_Physics::OnUpdate(Registry& registry, float dt) {
    if (!m_PhysicsSystem) return;

    // 1. 检测并创建新实体的 Jolt Body
    registry.view<C_D_Transform, C_D_RigidBody, C_D_Collider>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col) {
            if (!rb.body_created) {
                CreateBodyForEntity(registry, id, tf, rb, col);
            }
        }
    );

    // 1.5 首帧所有静态体添加完毕后，优化 BroadPhase（提升碰撞查询性能）
    if (!m_BroadPhaseOptimized) {
        m_PhysicsSystem->OptimizeBroadPhase();
        m_BroadPhaseOptimized = true;
        LOG_INFO("[Sys_Physics] OptimizeBroadPhase called");
    }

    // 2. 清理已销毁实体的孤立 Body
    DestroyOrphanBodies(registry);

    // 2.5 同步动力学参数（支持运行时修改）
    // 说明：静态体不参与动力学积分；运动学体不受重力影响，但可使用阻尼参数保持配置一致。
    {
        auto& bi = m_PhysicsSystem->GetBodyInterface();
        registry.view<C_D_RigidBody>().each([&](EntityID id, C_D_RigidBody& rb) {
            if (!rb.body_created || rb.is_static) return;

            JPH::BodyID jid(rb.jolt_body_id);
            bi.SetGravityFactor(jid, rb.gravity_factor);

            // 阻尼不在 BodyInterface 上直接暴露，需要写锁后通过 MotionProperties 设置
            JPH::BodyLockWrite lock(m_PhysicsSystem->GetBodyLockInterface(), jid);
            if (lock.Succeeded()) {
                JPH::Body& body = lock.GetBody();
                if (!body.IsStatic()) {
                    body.GetMotionProperties()->SetLinearDamping(rb.linear_damping);
                    body.GetMotionProperties()->SetAngularDamping(rb.angular_damping);
                }
            }
        });
    }

    // dt 当前由 SceneManager 的固定步长调度路径接管物理推进，此处保留参数以维持接口一致
    (void)dt;
}

// ============================================================
// OnFixedUpdate（为统一固定步长入口预留）
// ============================================================
void ECS::Sys_Physics::OnFixedUpdate(Registry& registry, float fixedDt) {
    if (!m_PhysicsSystem) return;
    if (fixedDt <= 0.0f) return;

    // 固定步长物理推进：后续由 SceneManager 统一调度该入口
    m_PhysicsSystem->Update(fixedDt, 1, m_TempAllocator.get(), m_JobSystem.get());

    // 固定步长后回写 ECS Transform，并分发本步碰撞事件
    SyncTransformsFromJolt(registry);
    FlushCollisionEvents(registry);
}

// ============================================================
// OnDestroy
// ============================================================
void ECS::Sys_Physics::OnDestroy(Registry& registry) {
    // 移除所有 Jolt Body
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    for (auto& [bodyID, entityID] : m_BodyToEntity) {
        JPH::BodyID jid(bodyID);
        bi.RemoveBody(jid);
        bi.DestroyBody(jid);
    }
    m_BodyToEntity.clear();

    // 析构顺序：PhysicsSystem → JobSystem → TempAllocator
    m_PhysicsSystem.reset();
    m_JobSystem.reset();
    m_TempAllocator.reset();

    // 清空并释放 EventBus：避免遗留事件在场景切换后被错误触发
    if (m_EventBus) {
        m_EventBus->clear();
        m_EventBus.reset();
    }

    // 断开 Registry 上下文中的裸指针，防止后续系统误用悬空指针
    if (registry.has_ctx<ECS::EventBus*>()) {
        registry.ctx<ECS::EventBus*>() = nullptr;
    }

    // Jolt 全局资源（Factory 等）保持存活，避免多系统场景问题
    m_BroadPhaseOptimized = false;
    LOG_INFO("[Sys_Physics] OnDestroy - Jolt PhysicsSystem destroyed");
}

// ============================================================
// CreateBodyForEntity
// ============================================================
void ECS::Sys_Physics::CreateBodyForEntity(
    Registry& reg, EntityID id,
    C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col)
{
    auto& bi = m_PhysicsSystem->GetBodyInterface();

    // --- 构建 Shape ---
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

    // --- 运动类型（统一优先级：Trigger/Static > Kinematic > Dynamic）---
    // Trigger 仅用于重叠检测，不需要动力学响应，因此按静态体创建。
    // 当 is_static 与 is_kinematic 同时为 true 时，以静态体为准。
    JPH::EMotionType motionType = JPH::EMotionType::Dynamic;
    JPH::ObjectLayer layer      = PhysicsLayers::MOVING;

    const bool forceStatic = rb.is_static || col.is_trigger;
    if (forceStatic) {
        motionType = JPH::EMotionType::Static;
        layer      = PhysicsLayers::NON_MOVING;

        if (rb.is_static && rb.is_kinematic) {
            LOG_WARN("[Sys_Physics] Entity " << id
                     << " has both is_static and is_kinematic; static takes precedence.");
        }
    } else if (rb.is_kinematic) {
        motionType = JPH::EMotionType::Kinematic;
        layer      = PhysicsLayers::MOVING;
    }

    // --- BodyCreationSettings ---
    JPH::BodyCreationSettings bcs(
        shapeResult.Get(),
        JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
        JPH::Quat(tf.rotation.x, tf.rotation.y, tf.rotation.z, tf.rotation.w),
        motionType,
        layer
    );

    // 质量与阻尼
    if (motionType == JPH::EMotionType::Dynamic) {
        bcs.mMassPropertiesOverride.mMass = rb.mass;
        bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        bcs.mLinearDamping  = rb.linear_damping;
        bcs.mAngularDamping = rb.angular_damping;
        bcs.mGravityFactor  = rb.gravity_factor;
    }

    // 摩擦与弹性（所有运动类型都可设置）
    bcs.mFriction    = col.friction;
    bcs.mRestitution = col.restitution;

    // Trigger 模式
    bcs.mIsSensor = col.is_trigger;

    // 旋转锁定（2.5D 游戏常用：锁定 X/Z 轴旋转）
    if (rb.lock_rotation_x || rb.lock_rotation_y || rb.lock_rotation_z) {
        JPH::EAllowedDOFs dofs = JPH::EAllowedDOFs::All;
        if (rb.lock_rotation_x) dofs = dofs & ~JPH::EAllowedDOFs::RotationX;
        if (rb.lock_rotation_y) dofs = dofs & ~JPH::EAllowedDOFs::RotationY;
        if (rb.lock_rotation_z) dofs = dofs & ~JPH::EAllowedDOFs::RotationZ;
        bcs.mAllowedDOFs = dofs;
    }

    // 存储 EntityID 到 Body UserData（用于碰撞回调反查实体）
    bcs.mUserData = (uint64_t)id;

    // --- 创建并激活 Body ---
    JPH::Body* body = bi.CreateBody(bcs);
    if (!body) {
        LOG_ERROR("[Sys_Physics] Failed to create Jolt Body for entity " << id);
        return;
    }

    bi.AddBody(body->GetID(),
               (motionType == JPH::EMotionType::Static)
                   ? JPH::EActivation::DontActivate
                   : JPH::EActivation::Activate);

    // 记录映射
    uint32_t rawID = body->GetID().GetIndexAndSequenceNumber();
    rb.jolt_body_id  = rawID;
    rb.body_created  = true;
    m_BodyToEntity[rawID] = id;
}

// ============================================================
// SyncTransformsFromJolt（动态体）
// ============================================================
void ECS::Sys_Physics::SyncTransformsFromJolt(Registry& reg) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();

    reg.view<C_D_Transform, C_D_RigidBody>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb) {
            // 只同步动态体（非静态、非运动学、已创建）
            if (!rb.body_created || rb.is_static || rb.is_kinematic) return;

            JPH::BodyID jid(rb.jolt_body_id);
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
void ECS::Sys_Physics::FlushCollisionEvents(Registry& reg) {
    // 从 ContactListener 取出 pending 事件（加锁后 swap）
    std::vector<ECSContactListener::PendingContact> contacts;
    {
        std::lock_guard lock(m_ContactListener.mutex);
        contacts.swap(m_ContactListener.pending);
    }
    if (contacts.empty()) return;

    // 场景拆卸期间可能已断开 EventBus 指针，此时直接丢弃积压事件以保证安全
    if (!reg.has_ctx<ECS::EventBus*>()) {
        return;
    }

    ECS::EventBus* bus = reg.ctx<ECS::EventBus*>();
    if (!bus) {
        return;
    }

    for (auto& c : contacts) {
        // 查找对应的 ECS 实体
        auto itA = m_BodyToEntity.find(c.body_id_a);
        auto itB = m_BodyToEntity.find(c.body_id_b);
        if (itA == m_BodyToEntity.end() || itB == m_BodyToEntity.end()) continue;

        EntityID entA = itA->second;
        EntityID entB = itB->second;

        if (c.is_trigger) {
            if (c.is_exit) {
                // TriggerExit
                Evt_Phys_TriggerExit evt;
                evt.entity_trigger = entA;
                evt.entity_other   = entB;
                bus->publish<Evt_Phys_TriggerExit>(evt);
            } else {
                // TriggerEnter
                Evt_Phys_TriggerEnter evt;
                evt.entity_trigger = entA;
                evt.entity_other   = entB;
                bus->publish<Evt_Phys_TriggerEnter>(evt);
            }
        } else {
            // 普通碰撞
            Evt_Phys_Collision evt;
            evt.entity_a            = entA;
            evt.entity_b            = entB;
            evt.contact_point       = Vector3(c.contact_x, c.contact_y, c.contact_z);
            evt.contact_normal      = Vector3(c.normal_x,  c.normal_y,  c.normal_z);
            evt.separating_velocity = c.separating_velocity;
            bus->publish<Evt_Phys_Collision>(evt);
        }
    }
}

// ============================================================
// DestroyOrphanBodies（清理已销毁实体的 Jolt Body）
// ============================================================
void ECS::Sys_Physics::DestroyOrphanBodies(Registry& reg) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    std::vector<uint32_t> toRemove;

    for (auto& [bodyID, entityID] : m_BodyToEntity) {
        // 如果实体不再有效（已销毁）
        if (!reg.Valid(entityID)) {
            toRemove.push_back(bodyID);
        }
    }

    for (uint32_t rawID : toRemove) {
        JPH::BodyID jid(rawID);

        // 移除 body 前，唤醒该区域内所有睡眠的动态体。
        // Jolt 的 RemoveBody 不会自动激活邻近体，导致叠放在上方的 cube
        // 因 sleep 状态永远不更新位置，视觉上悬浮在空中。
        if (bi.IsAdded(jid)) {
            JPH::AABox bounds = bi.GetTransformedShape(jid).GetWorldSpaceBounds();
            // 向上扩展，确保紧贴上方的 cube 也被覆盖到
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
