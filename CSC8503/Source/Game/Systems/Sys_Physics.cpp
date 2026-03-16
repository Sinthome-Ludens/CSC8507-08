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
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

using namespace NCL::Maths;

// ============================================================
// Jolt ↔ NCL 转换工具
// ============================================================
/**
 * @brief 将三个浮点分量转换为 Jolt Vec3。
 * @param x, y, z 浮点坐标分量
 * @return JPH::Vec3
 */
JPH::Vec3 ECS::Sys_Physics::ToJolt(float x, float y, float z) {
    return JPH::Vec3(x, y, z);
}
/**
 * @brief 将四元数分量转换为 Jolt Quat。
 * @param qx, qy, qz, qw 四元数分量
 * @return JPH::Quat
 */
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
 * @brief Jolt 全局初始化（只执行一次）：注册内存分配器、Factory、类型。
 */
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
/**
 * @brief 系统初始化：调用 InitJolt、创建 BroadPhaseLayerInterface/PhysicsSystem、
 *        设置碰撞/触发回调，并注册自身指针到 registry ctx。
 * @param registry ECS 注册表
 */
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

    // 注册 Sys_Physics* 到 ctx，供其他系统（如 Sys_Movement / Sys_StealthMetrics）访问物理接口
    // 无条件覆盖，防止场景切换后残留悬空指针
    // 注意：EventBus 由 SceneManager::EnterScene() 在 OnAwake 之前创建并注入 ctx，此处无需处理
    registry.ctx_emplace<Sys_Physics*>(this);

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

    // 1. 检测并创建新实体的 Jolt Body（标准碰撞体）
    registry.view<C_D_Transform, C_D_RigidBody, C_D_Collider>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb, C_D_Collider& col) {
            if (!rb.body_created) {
                CreateBodyForEntity(registry, id, tf, rb, col);
            }
        }
    );

    // 1b. 检测并创建三角网格碰撞体（用于多层地图地板/斜坡）
    registry.view<C_D_Transform, C_D_RigidBody, C_D_TriMeshCollider>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb, C_D_TriMeshCollider& tri) {
            if (!rb.body_created) {
                CreateTriMeshBodyForEntity(registry, id, tf, rb, tri);
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

            const auto itBody = m_EntityToBody.find(id);
            if (itBody == m_EntityToBody.end()) return;
            const uint32_t bodyID = itBody->second;
            const auto itPrev = g_LastGravityFactors.find(bodyID);
            const float previous = (itPrev != g_LastGravityFactors.end())
                ? itPrev->second
                : rb.gravity_factor;

            bi.SetGravityFactor(JPH::BodyID(bodyID), rb.gravity_factor);

            constexpr float EPS = 1e-4f;
            if (previous <= EPS && rb.gravity_factor > EPS) {
                bi.ActivateBody(JPH::BodyID(bodyID));
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
/**
 * @brief 销毁物理系统并释放当前场景创建的全部 Jolt 资源。
 * @details 逐个移除并销毁已注册 Body，清空实体与 Body 的双向映射及运行时缓存，然后撤销 registry context 中的物理系统裸指针。
 * @param registry 当前场景注册表
 */
void ECS::Sys_Physics::OnDestroy(Registry& registry) {
    // 移除所有 Jolt Body
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    for (auto& [bodyID, entityID] : m_BodyToEntity) {
        JPH::BodyID jid(bodyID);
        bi.RemoveBody(jid);
        bi.DestroyBody(jid);
    }
    m_BodyToEntity.clear();
    m_EntityToBody.clear();
    g_LastGravityFactors.clear();

    // 析构顺序：PhysicsSystem → JobSystem → TempAllocator
    m_PhysicsSystem.reset();
    m_JobSystem.reset();
    m_TempAllocator.reset();

    // 清除 ctx 中的裸指针，防止场景切换后悬空引用
    if (registry.has_ctx<Sys_Physics*>()) {
        registry.ctx_erase<Sys_Physics*>();
    }

    // Jolt 全局资源（Factory 等）保持存活，避免多系统场景问题
    m_BroadPhaseOptimized = false;
    LOG_INFO("[Sys_Physics] OnDestroy - Jolt PhysicsSystem destroyed");
}

// ============================================================
// CreateBodyForEntity
// ============================================================
/**
 * @brief 为指定实体创建 Jolt 刚体。
 * @details 根据变换、刚体和碰撞体组件构建 Shape 与 BodyCreationSettings，创建成功后把 EntityID 与底层 BodyID 建立双向映射。
 * @param reg 当前场景注册表
 * @param id 目标实体 ID
 * @param tf 目标实体变换组件
 * @param rb 目标实体刚体组件
 * @param col 目标实体碰撞体组件
 */
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
    rb.body_created  = true;
    m_BodyToEntity[rawID] = id;
    m_EntityToBody[id]    = rawID;
}

// ============================================================
// CreateTriMeshBodyForEntity — Jolt MeshShape（多层地图地板）
// ============================================================
/**
 * @brief 从 C_D_TriMeshCollider 数据创建 Jolt 静态 MeshShape 刚体。
 *
 * 仅用于静态地板/斜坡实体（is_static=true）。与 CreateBodyForEntity 的区别：
 * 使用 JPH::MeshShapeSettings 而非 Box/Sphere/Capsule，支持任意三角网格。
 *
 * @param reg  ECS Registry
 * @param id   目标实体 ID
 * @param tf   实体 Transform（提供世界偏移）
 * @param rb   RigidBody 组件（设置 body_created 标志）
 * @param tri  TriMeshCollider 组件（顶点 + 索引，必须非空且索引数为 3 的倍数）
 */
void ECS::Sys_Physics::CreateTriMeshBodyForEntity(
    Registry& reg, EntityID id,
    C_D_Transform& tf, C_D_RigidBody& rb, C_D_TriMeshCollider& tri)
{
    if (tri.vertices.empty() || tri.indices.empty()) {
        LOG_WARN("[Sys_Physics] CreateTriMeshBodyForEntity: empty geometry for entity " << id);
        rb.body_created = true;  // 防止每帧重复触发
        return;
    }
    const int triCount = static_cast<int>(tri.indices.size()) / 3;
    if (triCount == 0) return;

    auto& bi = m_PhysicsSystem->GetBodyInterface();

    // 构建 Jolt 顶点列表
    JPH::VertexList joltVerts;
    joltVerts.reserve(tri.vertices.size());
    for (const auto& v : tri.vertices) {
        joltVerts.push_back(JPH::Float3(v.x, v.y, v.z));
    }

    // 构建 Jolt 三角形索引列表
    JPH::IndexedTriangleList joltTris;
    joltTris.reserve(triCount);
    for (int i = 0; i < triCount; ++i) {
        joltTris.push_back(JPH::IndexedTriangle(
            static_cast<uint32_t>(tri.indices[i*3 + 0]),
            static_cast<uint32_t>(tri.indices[i*3 + 1]),
            static_cast<uint32_t>(tri.indices[i*3 + 2]),
            0));
    }

    JPH::MeshShapeSettings meshSettings(joltVerts, joltTris);
    auto shapeResult = meshSettings.Create();
    if (shapeResult.HasError()) {
        LOG_ERROR("[Sys_Physics] MeshShape creation failed for entity " << id
                  << ": " << shapeResult.GetError().c_str());
        return;
    }

    JPH::BodyCreationSettings bcs(
        shapeResult.Get(),
        JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        PhysicsLayers::NON_MOVING);

    bcs.mFriction    = tri.friction;
    bcs.mRestitution = tri.restitution;
    bcs.mIsSensor    = tri.is_trigger;   // Trigger 模式：仅触发事件，不产生物理推力
    bcs.mUserData    = static_cast<uint64_t>(id);

    JPH::Body* body = bi.CreateBody(bcs);
    if (!body) {
        LOG_ERROR("[Sys_Physics] Failed to create TriMesh Body for entity " << id);
        return;
    }

    bi.AddBody(body->GetID(), JPH::EActivation::DontActivate);

    uint32_t rawID   = body->GetID().GetIndexAndSequenceNumber();
    rb.body_created  = true;
    m_BodyToEntity[rawID] = id;
    m_EntityToBody[id]    = rawID;

    LOG_INFO("[Sys_Physics] TriMesh floor body created: entity=" << id
             << " tris=" << triCount
             << " pos=(" << tf.position.x << "," << tf.position.y << "," << tf.position.z << ")");
}

// ============================================================
// SyncTransformsFromJolt（动态体 Jolt→ECS / 运动学体 ECS→Jolt）
// ============================================================
/**
 * @brief 在 ECS Transform 与 Jolt 模拟结果之间同步位姿。
 * @details 对运动学体执行 ECS 到 Jolt 的 MoveKinematic，对动态体则把 Jolt 的位置和旋转回写到 ECS Transform。
 * @param reg 当前场景注册表
 * @param fixedDt 固定物理步长
 */
void ECS::Sys_Physics::SyncTransformsFromJolt(Registry& reg, float fixedDt) {
    auto& bi = m_PhysicsSystem->GetBodyInterface();

    reg.view<C_D_Transform, C_D_RigidBody>().each(
        [&](EntityID id, C_D_Transform& tf, C_D_RigidBody& rb) {
            if (!rb.body_created || rb.is_static) return;

            JPH::BodyID jid;
            if (!TryGetBodyID(id, jid)) return;

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

/**
 * @brief 尝试从实体 ID 解析对应的 Jolt BodyID。
 * @details 仅查询 Sys_Physics 内部维护的反向映射，不触发 Body 创建；若实体不存在映射项，则返回 false 并保持调用方可安全跳过本次物理操作。
 * @param entity 输入的 ECS 实体 ID
 * @param outBodyID 成功时输出对应的 Jolt BodyID
 * @return 找到有效映射返回 true，否则返回 false
 */
bool ECS::Sys_Physics::TryGetBodyID(EntityID entity, JPH::BodyID& outBodyID) const {
    const auto it = m_EntityToBody.find(entity);
    if (it == m_EntityToBody.end()) return false;
    outBodyID = JPH::BodyID(it->second);
    return true;
}

// ============================================================
// FlushCollisionEvents
// ============================================================
/**
 * @brief 将挂起的接触事件转换为 ECS 事件并发布。
 * @details 从 ContactListener 提取缓存的碰撞与触发器事件，解析涉及实体后发布对应的 TriggerEnter、TriggerExit 或普通碰撞事件。
 * @param reg 当前场景注册表
 */
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

        // 使用 ContactListener 直接从 Jolt body.IsSensor() 获取的标志
        // 不再依赖 C_D_Collider 组件重查（兼容 C_D_TriMeshCollider 触发器）
        if (c.is_trigger) {
            // 确定哪个是触发器实体：检查 C_D_Collider 或 C_D_TriMeshCollider
            bool isTriggerA = (reg.Has<C_D_Collider>(entA) && reg.Get<C_D_Collider>(entA).is_trigger)
                           || (reg.Has<C_D_TriMeshCollider>(entA) && reg.Get<C_D_TriMeshCollider>(entA).is_trigger);
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

// ============================================================
// DestroyOrphanBodies（清理已销毁实体的 Jolt Body）
// ============================================================
/**
 * @brief 清理无效实体遗留的孤立 Jolt Body。
 * @details 扫描 Body 到 Entity 的映射，销毁所有已经失效实体对应的 Body，并同步清理反向映射与运行时缓存，避免 stale 映射被后续复用命中。
 * @param reg 当前场景注册表
 */
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
        const auto itEntity = m_BodyToEntity.find(rawID);
        const EntityID entity = (itEntity != m_BodyToEntity.end())
            ? itEntity->second
            : Entity::NULL_ENTITY;

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
        auto itE = m_BodyToEntity.find(rawID);
        if (itE != m_BodyToEntity.end()) {
            m_EntityToBody.erase(itE->second);
        }
        m_BodyToEntity.erase(rawID);
        g_LastGravityFactors.erase(rawID);
    }
}

// ============================================================
// 工具函数（统一按 EntityID 操作，内部通过 TryGetBodyID 查找 Jolt Body）
// ============================================================

/**
 * @brief 按实体 ID 设置线速度。
 * @details 若实体未绑定有效刚体或物理系统尚未初始化，则直接返回，不产生副作用。
 * @param entity 目标实体 ID
 * @param vx X 方向速度
 * @param vy Y 方向速度
 * @param vz Z 方向速度
 */
void ECS::Sys_Physics::SetLinearVelocity(EntityID entity, float vx, float vy, float vz) {
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    m_PhysicsSystem->GetBodyInterface().SetLinearVelocity(bodyID, ToJolt(vx, vy, vz));
}

/**
 * @brief 按实体 ID 施加冲量。
 * @details 若实体没有有效 Body 映射，则忽略本次调用。
 * @param entity 目标实体 ID
 * @param ix X 方向冲量
 * @param iy Y 方向冲量
 * @param iz Z 方向冲量
 */
void ECS::Sys_Physics::ApplyImpulse(EntityID entity, float ix, float iy, float iz) {
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    m_PhysicsSystem->GetBodyInterface().AddImpulse(bodyID, ToJolt(ix, iy, iz));
}

/**
 * @brief 按实体 ID 施加持续力。
 * @details 该接口通常被逐帧调用，以便让动态体持续受到外力驱动。
 * @param entity 目标实体 ID
 * @param fx X 方向力
 * @param fy Y 方向力
 * @param fz Z 方向力
 */
void ECS::Sys_Physics::AddForce(EntityID entity, float fx, float fy, float fz) {
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    m_PhysicsSystem->GetBodyInterface().AddForce(bodyID, ToJolt(fx, fy, fz));
}

/**
 * @brief 按实体 ID 读取线速度。
 * @details 若目标实体没有有效刚体映射，则返回零向量。
 * @param entity 目标实体 ID
 * @return 当前线速度
 */
Vector3 ECS::Sys_Physics::GetLinearVelocity(EntityID entity) {
    if (!m_PhysicsSystem) return Vector3(0, 0, 0);
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return Vector3(0, 0, 0);
    JPH::Vec3 v = m_PhysicsSystem->GetBodyInterface().GetLinearVelocity(bodyID);
    return Vector3(v.GetX(), v.GetY(), v.GetZ());
}

/**
 * @brief 按实体 ID 驱动运动学刚体移动。
 * @details 根据传入目标位姿与固定步长，把 EntityID 解析为底层 Body 后调用 Jolt 的 MoveKinematic。
 * @param entity 目标实体 ID
 * @param px 目标位置 X
 * @param py 目标位置 Y
 * @param pz 目标位置 Z
 * @param qx 目标旋转 X
 * @param qy 目标旋转 Y
 * @param qz 目标旋转 Z
 * @param qw 目标旋转 W
 * @param dt 本次运动学更新步长
 */
void ECS::Sys_Physics::MoveKinematic(
    EntityID entity,
    float px, float py, float pz,
    float qx, float qy, float qz, float qw,
    float dt)
{
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    m_PhysicsSystem->GetBodyInterface().MoveKinematic(
        bodyID,
        JPH::RVec3(px, py, pz),
        JPH::Quat(qx, qy, qz, qw),
        dt);
}

// ============================================================
// SetRotation（供 Sys_Navigation 调用 - 来自 feat/navmesh-system）
// ============================================================
/**
 * @brief 按实体 ID 设置刚体旋转。
 * @details 主要用于导航等系统同步刚体朝向，并在设置后立即激活目标 Body。
 * @param entity 目标实体 ID
 * @param rotation 目标旋转
 */
void ECS::Sys_Physics::SetRotation(EntityID entity,
                                   const NCL::Maths::Quaternion& rotation)
{
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    m_PhysicsSystem->GetBodyInterface().SetRotation(
        bodyID,
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
// CastRayIgnoring — 射线检测，忽略指定实体的碰撞体
// ============================================================
ECS::Sys_Physics::RaycastHit ECS::Sys_Physics::CastRayIgnoring(
    float ox, float oy, float oz,
    float dx, float dy, float dz,
    float maxDist,
    EntityID ignoreA, EntityID ignoreB)
{
    RaycastHit result{};
    if (!m_PhysicsSystem) return result;

    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-6f) return result;
    dx /= len; dy /= len; dz /= len;

    // 查找需要忽略的 Jolt BodyID
    JPH::BodyID ignoreBodyA, ignoreBodyB;
    {
        auto it = m_EntityToBody.find(ignoreA);
        if (it != m_EntityToBody.end())
            ignoreBodyA = JPH::BodyID(it->second);
    }
    if (ignoreB != Entity::NULL_ENTITY) {
        auto it = m_EntityToBody.find(ignoreB);
        if (it != m_EntityToBody.end())
            ignoreBodyB = JPH::BodyID(it->second);
    }

    // BodyFilter：排除指定实体
    struct IgnoreFilter : public JPH::BodyFilter {
        JPH::BodyID a, b;
        bool ShouldCollide(const JPH::BodyID& inBodyID) const override {
            return inBodyID != a && inBodyID != b;
        }
        bool ShouldCollideLocked(const JPH::Body&) const override { return true; }
    };
    IgnoreFilter filter;
    filter.a = ignoreBodyA;
    filter.b = ignoreBodyB;

    JPH::RRayCast ray(JPH::RVec3(ox, oy, oz),
                      JPH::Vec3(dx * maxDist, dy * maxDist, dz * maxDist));
    JPH::RayCastResult hit;
    hit.mFraction = 1.0f + FLT_EPSILON;

    const auto& query = m_PhysicsSystem->GetNarrowPhaseQuery();
    if (query.CastRay(ray, hit, JPH::BroadPhaseLayerFilter(),
                      JPH::ObjectLayerFilter(), filter)) {
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
    }
    return result;
}

// ============================================================
// ReplaceShapeCapsule — 运行时替换碰撞体为 Capsule (来自 master)
// ============================================================
/**
 * @brief 按实体 ID 替换为 Capsule 碰撞体。
 * @details 用于玩家姿态切换等运行时形状调整场景，保留现有质量属性并激活目标 Body。
 * @param entity 目标实体 ID
 * @param halfHeight Capsule 半高
 * @param radius Capsule 半径
 */
void ECS::Sys_Physics::ReplaceShapeCapsule(EntityID entity, float halfHeight, float radius) {
    if (!m_PhysicsSystem) return;
    JPH::BodyID jid;
    if (!TryGetBodyID(entity, jid)) return;
    auto& bi = m_PhysicsSystem->GetBodyInterface();
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
/**
 * @brief 按实体 ID 设置刚体世界位置。
 * @details 设置位置后会立即激活目标 Body，确保位移在后续模拟中马上生效。
 * @param entity 目标实体 ID
 * @param px X 坐标
 * @param py Y 坐标
 * @param pz Z 坐标
 */
void ECS::Sys_Physics::SetPosition(EntityID entity, float px, float py, float pz) {
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    auto& bi = m_PhysicsSystem->GetBodyInterface();
    bi.SetPosition(bodyID, JPH::RVec3(px, py, pz), JPH::EActivation::Activate);
}
// ============================================================
// ActivateBody — 强制唤醒 Body (来自 master)
// ============================================================
/**
 * @brief 按实体 ID 强制唤醒刚体。
 * @details 用于确保睡眠中的刚体能立即响应后续的力、速度或位置更新。
 * @param entity 目标实体 ID
 */
void ECS::Sys_Physics::ActivateBody(EntityID entity) {
    if (!m_PhysicsSystem) return;
    JPH::BodyID bodyID;
    if (!TryGetBodyID(entity, bodyID)) return;
    m_PhysicsSystem->GetBodyInterface().ActivateBody(bodyID);
}
