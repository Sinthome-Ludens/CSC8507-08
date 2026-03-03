#include "PrefabFactory.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_NavAgent.h"
#include "Game/Components/C_T_Pathfinder.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Utils/Log.h"

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

// ============================================================
// CreatePhysicsEnemy  →  PREFAB_PHYSICS_ENEMY
// ============================================================
EntityID PrefabFactory::CreatePhysicsEnemy(
    Registry&       reg,
    ECS::MeshHandle enemyMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        enemyMesh,
        static_cast<uint32_t>(0)
    );

    C_D_RigidBody rb{};
    rb.mass            = 1.0f;
    rb.gravity_factor  = 1.0f;
    rb.lock_rotation_x = true;  // 防止胶囊体前后翻转
    rb.lock_rotation_z = true;  // 防止胶囊体左右翻转
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type   = ColliderType::Capsule;
    col.half_x = 0.5f;  // radius
    col.half_y = 1.0f;  // half_height
    reg.Emplace<C_D_Collider>(entity, col);

    // EnemyAI 核心组件
    reg.Emplace<C_T_Enemy>(entity);
    reg.Emplace<C_D_AIState>(entity);

    auto& detect = reg.Emplace<C_D_AIPerception>(entity);
    detect.detection_value          = 0.0f;
    detect.detection_value_increase = 15.0f;
    detect.detection_value_decrease = 5.0f;

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Enemy_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreatePhysicsEnemy id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreateNavEnemy  →  PREFAB_NAV_ENEMY
// ============================================================
EntityID PrefabFactory::CreateNavEnemy(
    Registry&       reg,
    ECS::MeshHandle enemyMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        enemyMesh,
        static_cast<uint32_t>(0)
    );

    C_D_RigidBody rb{};
    rb.mass            = 1.0f;
    rb.gravity_factor  = 1.0f;
    rb.lock_rotation_x = true;
    rb.lock_rotation_z = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type   = ColliderType::Capsule;
    col.half_x = 0.5f;
    col.half_y = 1.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    // EnemyAI 核心组件（状态机）
    reg.Emplace<C_T_Enemy>(entity);
    reg.Emplace<C_D_AIState>(entity);

    auto& detect = reg.Emplace<C_D_AIPerception>(entity);
    detect.detection_value          = 0.0f;
    detect.detection_value_increase = 15.0f;
    detect.detection_value_decrease = 5.0f;

    // NavAgent 导航组件
    reg.Emplace<C_D_NavAgent>(entity);      // 使用默认参数（speed=5, search_tag="Player"）
    reg.Emplace<C_T_Pathfinder>(entity);    // 启用寻路标签

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Nav_Enemy_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreateNavEnemy id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreateNavTarget  →  PREFAB_NAV_TARGET
// ============================================================
EntityID PrefabFactory::CreateNavTarget(
    Registry&       reg,
    ECS::MeshHandle targetMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(0.5f, 0.5f, 0.5f)  // 较小的目标方块
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        targetMesh,
        static_cast<uint32_t>(0)
    );

    // 静态体（不会被物理推动）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type   = ColliderType::Box;
    col.half_x = 0.5f;
    col.half_y = 0.5f;
    col.half_z = 0.5f;
    reg.Emplace<C_D_Collider>(entity, col);

    // NavTarget 标签（searchTag="Player" 默认匹配）
    auto& target = reg.Emplace<C_T_NavTarget>(entity);
    strncpy_s(target.target_type, sizeof(target.target_type), "Player", sizeof(target.target_type) - 1);

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Nav_Target_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreateNavTarget id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

