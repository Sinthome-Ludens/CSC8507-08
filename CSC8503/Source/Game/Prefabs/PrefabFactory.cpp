#include "PrefabFactory.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Utils/Log.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPreception.h"

#include <cstring>
#include <cstdio>

using namespace NCL::Maths;
using namespace ECS;

// ============================================================
// 内部辅助：挂载 C_D_DebugName（规范要求所有实体必须挂载）
// ============================================================
static void AttachDebugName(Registry& reg, EntityID id, const char* name) {
    auto& dn = reg.Emplace<C_D_DebugName>(id);
    strncpy_s(dn.name, sizeof(C_D_DebugName::name), name, sizeof(C_D_DebugName::name) - 1);
}

// ============================================================
// CreateCameraMain  →  PREFAB_CAMERA_MAIN
// ============================================================
EntityID PrefabFactory::CreateCameraMain(
    Registry&   reg,
    Vector3     position,
    float       pitch,
    float       yaw)
{
    EntityID entity = reg.Create();

    // C_D_Transform（位置由参数覆盖；旋转/缩放使用默认值）
    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_Camera（投影参数 + 视角控制，匹配 Prefab_Camera_Main.json）
    C_D_Camera cam{};
    cam.fov         = 45.0f;
    cam.near_z      = 1.0f;
    cam.far_z       = 1000.0f;
    cam.pitch       = pitch;
    cam.yaw         = yaw;
    cam.move_speed  = 20.0f;
    cam.sensitivity = 0.5f;
    reg.Emplace<C_D_Camera>(entity, cam);

    // C_T_MainCamera（标签：标记为主相机，场景中唯一）
    reg.Emplace<C_T_MainCamera>(entity);

    // C_D_DebugName（规范必选）
    AttachDebugName(reg, entity, "ENTITY_Camera_Main");

    LOG_INFO("[PrefabFactory] CreateCameraMain id=" << entity
             << " pos=(" << position.x << "," << position.y << "," << position.z
             << ") pitch=" << pitch << " yaw=" << yaw);

    return entity;
}

// ============================================================
// CreateFloor  →  PREFAB_ENV_FLOOR
// ============================================================
EntityID PrefabFactory::CreateFloor(Registry& reg, ECS::MeshHandle cubeMesh)
{
    EntityID entity = reg.Create();

    // C_D_Transform（大平面，低于原点，匹配 Prefab_Env_Floor.json）
    reg.Emplace<C_D_Transform>(entity,
        Vector3(0.0f, -6.0f, 0.0f),
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(50.0f, 1.0f, 50.0f)
    );

    // C_D_MeshRenderer
    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_RigidBody（静态体）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Box，半尺寸 = mesh顶点范围±1 × scale）
    // cube.obj 顶点 ±1.0，scale (50,1,50) → 世界半尺寸 (50, 1, 50)
    C_D_Collider col{};
    col.type   = ColliderType::Box;
    col.half_x = 50.0f;
    col.half_y = 1.0f;
    col.half_z = 50.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    // C_D_DebugName（规范必选）
    AttachDebugName(reg, entity, "ENTITY_Env_Floor_Main");

    LOG_INFO("[PrefabFactory] CreateFloor id=" << entity);

    return entity;
}

// 在 PrefabFactory.cpp 中添加
// ============================================================
// CreateLevel  →  PREFAB_ENV_LEVEL (用于 Level1.obj)
// ============================================================
EntityID PrefabFactory::CreateLevel1(Registry& reg, ECS::MeshHandle levelMesh)
{
    EntityID entity = reg.Create();

    // 1. Transform: 使用 1.0 缩放，位置重置为原点
    // Level1.obj 的顶点已经包含了关卡布局，不需要像 cube 那样放大
    reg.Emplace<C_D_Transform>(entity,
        Vector3(0.0f, 0.0f, 0.0f),
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // 2. MeshRenderer
    reg.Emplace<C_D_MeshRenderer>(entity,
        levelMesh,
        static_cast<uint32_t>(0)
    );

    // 3. RigidBody: 必须设为静态体
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // 4. Collider: 注意！Level1 是复杂模型
    // 如果你的物理引擎支持 MeshCollider，应使用 Mesh 类型。
    // 这里暂时使用较大的 Box 作为底层物理边界，或根据需要配置
    C_D_Collider col{};
    col.type   = ColliderType::Box;
    col.half_x = 20.0f; // 根据 Level1.obj 顶点范围调整
    col.half_y = 1.0f;
    col.half_z = 20.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    // 5. DebugName
    AttachDebugName(reg, entity, "ENTITY_Env_Level_Main");

    LOG_INFO("[PrefabFactory] CreateLevel id=" << entity << " with default scale (1,1,1)");

    return entity;
}

// ============================================================
// CreatePhysicsCube  →  PREFAB_PHYSICS_CUBE
// ============================================================
EntityID PrefabFactory::CreatePhysicsCube(
    Registry&       reg,
    ECS::MeshHandle cubeMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    // C_D_Transform（使用调用方传入的世界坐标）
    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_MeshRenderer
    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_RigidBody（动态体，匹配 Prefab_Physics_Cube.json）
    C_D_RigidBody rb{};
    rb.mass            = 1.0f;
    rb.gravity_factor  = 1.0f;
    rb.linear_damping  = 0.05f;
    rb.angular_damping = 0.05f;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Box 1×1×1，cube.obj 顶点 ±1.0，scale (1,1,1) → 世界半尺寸 (1,1,1)）
    C_D_Collider col{};
    col.type        = ColliderType::Box;
    col.half_x      = 1.0f;
    col.half_y      = 1.0f;
    col.half_z      = 1.0f;
    col.friction    = 0.5f;
    col.restitution = 0.1f;
    reg.Emplace<C_D_Collider>(entity, col);

    // C_D_DebugName（含序号，匹配 ENTITY_Physics_Cube_XX 规范）
    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Physics_Cube_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreatePhysicsCube id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

EntityID PrefabFactory::CreatePhysicsCapsule(
    Registry&       reg,
    ECS::MeshHandle capsuleMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    // C_D_Transform（使用调用方传入的世界坐标）
    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_MeshRenderer
    reg.Emplace<C_D_MeshRenderer>(entity,
        capsuleMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_RigidBody（动态体）
    C_D_RigidBody rb{};
    rb.mass            = 1.0f;
    rb.gravity_factor  = 1.0f;
    rb.linear_damping  = 0.05f;
    rb.angular_damping = 0.05f;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Capsule：half_x = radius, half_y = half_height of cylinder part）
    // Capsule.msh 總高度 = 2.0：2 * half_height(0.5) + 2 * radius(0.5) = 2.0
    C_D_Collider col{};
    col.type        = ColliderType::Capsule;
    col.half_x      = 0.5f;   // radius
    col.half_y      = 0.5f;   // half_height（不含半球），與 Capsule.msh 尺寸匹配
    col.friction    = 0.5f;
    col.restitution = 0.0f;   // 消除圓底彈跳
    reg.Emplace<C_D_Collider>(entity, col);

    // C_D_DebugName（含序号，匹配 ENTITY_Physics_Cube_XX 规范）
    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Physics_Capsule_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreatePhysicsCapsule id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreatePhysicsCube  →  PREFAB_PHYSICS_Enemy
// ============================================================
EntityID PrefabFactory::CreatePhysicsEnemy(
Registry&   reg,
MeshHandle  enemyMesh,
int         spawnIndex,
Vector3     spawnPos
) {
    // 1. 创建实体根对象
    EntityID entity = reg.Create();

    // 2. 挂载基础空间组件 (C_D_Transform)
    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // 3. 挂载渲染组件 (C_D_MeshRenderer)
    reg.Emplace<C_D_MeshRenderer>(entity,
        enemyMesh,
        static_cast<uint32_t>(0) // 默认材质
    );

    // 4. 挂载物理组件 (参考 Sys_Physics 需求)
    C_D_RigidBody rb{};
    rb.mass = 1.0f;
    rb.gravity_factor = 1.0f;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type = ColliderType::Capsule; // 敌人通常使用胶囊体碰撞
    col.half_x = 0.5f; // 半径
    col.half_y = 1.0f; // 半高
    reg.Emplace<C_D_Collider>(entity, col);

    // 5. 挂载 AI 核心组件 (根据我们之前拆分的文件)
    reg.Emplace<C_T_Enemy>(entity);      // 身份标签
    reg.Emplace<C_D_AIState>(entity);    // 默认状态为 Safe

    auto& detect = reg.Emplace<C_D_AIPreception>(entity);
    detect.detectionValue = 0.0f;
    detect.detectionValueIncrease = 15.0f; // 可根据预制体类型调整

    // 6. 挂载调试名 (规范要求：SYS_ 索敌或调试时必须可见)
    char nameBuffer[32];
    sprintf_s(nameBuffer, "Enemy_Basic_%02d", spawnIndex);
    AttachDebugName(reg, entity, nameBuffer);

    return entity;
}

