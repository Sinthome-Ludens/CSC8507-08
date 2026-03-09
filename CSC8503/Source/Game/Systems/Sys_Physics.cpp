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
    // 无条件覆盖：场景重进时 registry.Clear() 不清 ctx，旧指针可能悬空
    m_EventBus = std::make_unique<ECS::EventBus>();
    registry.ctx_emplace<ECS::EventBus*>(m_EventBus.get());

    // 注册 Sys_Physics* 到 ctx，供其他系统（如 Sys_Movement / Sys_StealthMetrics）访问物理接口
    // 无条件覆盖，防止场景切换后残留悬空指针
    registry.ctx_emplace<Sys_Physics*>(this);

    // 注：Sys_Physics* 和 ECS::Sys_Physics* 是同一类型，line 99 已注册

    LOG_INFO("[Sys_Physics] OnAwake - Jolt PhysicsSystem initialized");
}

// ============================================================
// OnUpdate（Body 管理 + 参数同步，不执行步进）
// ============================================================
/**
 * @brief 每渲染帧调用：创建新 Body、清理孤立 Body、同步 gravity_factor。
 * @details
 * 不执行 Jolt 步进；物理积分由 OnFixedUpdate 在固定步长帧中负责。
 * 所有新增/移除实体的 Body 管理在此帧完成，确保物理步进时 Body 集合已就绪。
 * @param registry 当前场景注册表
 * @param dt       本帧变步长时间（秒，仅用于 gravity_factor 判断，不传入 Jolt）
 */
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

    // 2.5 同步 gravity_factor（支持运行时修改，如 ImGui 按钮开关重力）
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
    // 步进、Transform 同步、碰撞事件发布均由 OnFixedUpdate 负责
}

// ============================================================
// OnFixedUpdate（单次 Jolt 步进 + Transform 同步 + 事件发布）
// ============================================================
/**
 * @brief 每物理帧调用（固定步长），由 SceneManager 外部累加器驱动。
 * @details
 * 执行单次 Jolt 步进，步长 = fixedDt（由 SceneManager 从 Res_Time::fixedDeltaTime 传入）。
 * 步进完成后同步 Jolt 结果至 C_D_Transform，并将碰撞/触发事件发布到 EventBus。
 * 运动学体的 MoveKinematic 也使用相同的 fixedDt，保证速度计算一致。
 * @param registry 当前场景注册表
 * @param fixedDt  固定物理帧步长（秒），应等于 Res_Time::fixedDeltaTime
 */
void ECS::Sys_Physics::OnFixedUpdate(Registry& registry, float fixedDt) {
    if (!m_PhysicsSystem) return;

    // 单次步进（频率由 SceneManager 外部累加器保证为 60 Hz）
    m_PhysicsSystem->Update(fixedDt, 1, m_TempAllocator.get(), m_JobSystem.get());

    // 同步 Jolt 结果 → C_D_Transform（传入 fixedDt 供运动学体 MoveKinematic 使用）
    SyncTransformsFromJolt(registry, fixedDt);

    // 发布碰撞/触发事件到 EventBus
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
    g_LastGravityFactors.clear();

    // 析构顺序：PhysicsSystem → JobSystem → TempAllocator
    m_PhysicsSystem.reset();
    m_JobSystem.reset();
    m_TempAllocator.reset();

    // 清除 ctx 中的裸指针，防止场景切换后悬空引用
    if (registry.has_ctx<Sys_Physics*>()) {
        registry.ctx_erase<Sys_Physics*>();
    }

    // 如果此系统拥有并注册了 EventBus，将其从上下文中移除，防止留下悬空指针
    if (registry.has_ctx<ECS::EventBus*>()) {
        ECS::EventBus* bus = registry.ctx<ECS::EventBus*>();
        if (m_EventBus && bus == m_EventBus.get()) {
            registry.ctx_erase<ECS::EventBus*>();
        }
    }
    m_EventBus.reset();

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

    // --- 运动类型 ---
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
// SyncTransformsFromJolt（动态体 Jolt→ECS / 运动学体 ECS→Jolt）
// ============================================================
void ECS::Sys_Physics::SyncTransformsFromJolt(Registry& reg, float fixedDt) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();

    reg.view<C_D_Transform, C_D_RigidBody>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb) {
            if (!rb.body_created || rb.is_static) return;

            JPH::BodyID jid(rb.jolt_body_id);

            if (rb.is_kinematic) {
                // 运动学体：将 ECS Transform（由插值/输入驱动）写回 Jolt，
                // 使其在物理世界中实际移动，从而能与动态体产生碰撞。
                bi.MoveKinematic(
                    jid,
                    JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
                    JPH::Quat(tf.rotation.x, tf.rotation.y, tf.rotation.z, tf.rotation.w),
                    fixedDt);
                return;
            }

            // 动态体：Jolt → ECS
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

    auto& bus = *reg.ctx<ECS::EventBus*>();

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
                bus.publish<Evt_Phys_TriggerExit>(evt);
            } else {
                // TriggerEnter
                Evt_Phys_TriggerEnter evt;
                evt.entity_trigger = entA;
                evt.entity_other   = entB;
                bus.publish<Evt_Phys_TriggerEnter>(evt);
            }
        } else {
            // 普通碰撞
            Evt_Phys_Collision evt;
            evt.entity_a            = entA;
            evt.entity_b            = entB;
            evt.contact_point       = Vector3(c.contact_x, c.contact_y, c.contact_z);
            evt.contact_normal      = Vector3(c.normal_x,  c.normal_y,  c.normal_z);
            evt.separating_velocity = c.separating_velocity;
            bus.publish<Evt_Phys_Collision>(evt);
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
        // 实体已销毁 或 C_D_RigidBody 已被移除（死亡动画期间）
        if (!reg.Valid(entityID) || !reg.Has<C_D_RigidBody>(entityID)) {
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
ECS::Sys_Physics::RaycastHit ECS::Sys_Physics::CastRay(
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    float maxDist)
{
    RaycastHit result{};
    if (!m_PhysicsSystem) return result;
    // 归一化方向向量，防止调用方传入非单位向量
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-6f) return result;   // 零方向，直接返回未命中
    dx /= len;
    dy /= len;
    dz /= len;
    // 构造 Jolt 射线（方向向量长度 = maxDist，fraction 1.0 = 最大距离处）
    JPH::RRayCast ray(JPH::RVec3(ox, oy, oz),
                      JPH::Vec3(dx * maxDist, dy * maxDist, dz * maxDist));
    JPH::RayCastResult hit;
    hit.mFraction = 1.0f + FLT_EPSILON; // 初始化为未命中
    const auto& query = m_PhysicsSystem->GetNarrowPhaseQuery();
    if (query.CastRay(ray, hit)) {
        result.hit      = true;
        result.fraction = hit.mFraction;
        const uint32_t rawBodyID = hit.mBodyID.GetIndexAndSequenceNumber();
        const auto itEntity = m_BodyToEntity.find(rawBodyID);
        if (itEntity != m_BodyToEntity.end()) {
            result.entity = itEntity->second;
        }
        // 计算命中点
        JPH::RVec3 hitPoint = ray.GetPointOnRay(hit.mFraction);
        result.pointX = static_cast<float>(hitPoint.GetX());
        result.pointY = static_cast<float>(hitPoint.GetY());
        result.pointZ = static_cast<float>(hitPoint.GetZ());
        // 获取命中面的法线（需要 BodyLock 访问 Body 对象）
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
void ECS::Sys_Physics::ReplaceShapeCapsule(uint32_t joltBodyID, float halfHeight, float radius) {
    if (!m_PhysicsSystem) return;
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    JPH::BodyID jid(joltBodyID);
    // 创建新的 Capsule 形状
    JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
    auto shapeResult = capsuleSettings.Create();
    if (shapeResult.HasError()) {
        LOG_ERROR("[Sys_Physics] ReplaceShapeCapsule failed: " << shapeResult.GetError().c_str());
        return;
    }
    // 替换形状，保留原有质量属性（false = 不用新形状默认密度重算质量）
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
