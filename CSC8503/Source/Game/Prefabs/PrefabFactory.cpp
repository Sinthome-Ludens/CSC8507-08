#include "PrefabFactory.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Utils/Log.h"

#include <cstring>
#include <cstdio>
#include <cstdint>

using namespace NCL::Maths;
using namespace ECS;

namespace {
constexpr uint32_t LAYER_ENV     = 1u << 0;
constexpr uint32_t LAYER_DYNAMIC = 1u << 1;
constexpr uint32_t LAYER_TRIGGER = 1u << 2;

constexpr uint32_t TAG_DEFAULT   = 1u << 0;
constexpr uint32_t TAG_STICKY    = 1u << 1;
constexpr uint32_t TAG_SLIPPERY  = 1u << 2;
constexpr uint32_t TAG_BOUNCY    = 1u << 3;
constexpr uint32_t TAG_SENSOR    = 1u << 4;
}

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
    col.layer_mask = LAYER_ENV;
    col.tag_mask   = TAG_DEFAULT;
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

    // 旋转锁定分组（用于 2.5D 约束验收）：
    // 0组：锁 X/Z，仅允许绕 Y 轴旋转（典型 2.5D）
    // 1组：锁全部旋转轴（用于验证完全禁止旋转）
    // 2组：不锁旋转（作为对照组）
    const int rotationLockGroup = spawnIndex % 3;
    if (rotationLockGroup == 0) {
        rb.lock_rotation_x = true;
        rb.lock_rotation_z = true;
    } else if (rotationLockGroup == 1) {
        rb.lock_rotation_x = true;
        rb.lock_rotation_y = true;
        rb.lock_rotation_z = true;
    }

    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Box 1×1×1，cube.obj 顶点 ±1.0，scale (1,1,1) → 世界半尺寸 (1,1,1)）
    C_D_Collider col{};
    col.type        = ColliderType::Box;
    col.half_x      = 1.0f;
    col.half_y      = 1.0f;
    col.half_z      = 1.0f;

    // 材质分组（用于基础动力学验收）：
    // 0组：高摩擦、低弹性（更容易停下）
    // 1组：低摩擦、中弹性（更容易滑动+可见回弹）
    // 2组：中摩擦、高弹性（明显弹跳）
    const int materialGroup = spawnIndex % 3;
    if (materialGroup == 0) {
        col.friction    = 0.9f;
        col.restitution = 0.05f;
    } else if (materialGroup == 1) {
        col.friction    = 0.1f;
        col.restitution = 0.35f;
    } else {
        col.friction    = 0.5f;
        col.restitution = 0.8f;
    }

    const int queryGroup = spawnIndex % 5;
    if (queryGroup == 4) {
        col.is_trigger = true;
        col.layer_mask = LAYER_TRIGGER;
        col.tag_mask   = TAG_SENSOR;
    } else {
        col.layer_mask = LAYER_DYNAMIC;
        if (materialGroup == 0) {
            col.tag_mask = TAG_STICKY;
        } else if (materialGroup == 1) {
            col.tag_mask = TAG_SLIPPERY;
        } else {
            col.tag_mask = TAG_BOUNCY;
        }
    }

    reg.Emplace<C_D_Collider>(entity, col);

    // C_D_DebugName（含序号，匹配 ENTITY_Physics_Cube_XX 规范）
    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Physics_Cube_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreatePhysicsCube id=" << entity
             << " index=" << spawnIndex
             << " rot_lock_group=" << rotationLockGroup
              << " lock_xyz=(" << rb.lock_rotation_x << ","
              << rb.lock_rotation_y << "," << rb.lock_rotation_z << ")"
             << " query_group=" << queryGroup
             << " material_group=" << materialGroup
             << " trigger=" << col.is_trigger
             << " layer_mask=0x" << std::hex << col.layer_mask
             << " tag_mask=0x" << col.tag_mask << std::dec
             << " friction=" << col.friction
             << " restitution=" << col.restitution
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}
