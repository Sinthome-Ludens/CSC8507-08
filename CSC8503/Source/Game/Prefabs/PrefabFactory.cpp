/**
 * @file PrefabFactory.cpp
 * @brief 实体预制体工厂实现：各 Prefab 的组件组装与物理/渲染对齐策略
 *
 * @details
 * ## 胶囊体渲染缩放说明
 *
 * 项目所用的 Capsule.obj（来自 Assets/Meshes/）原始尺寸未归一化：
 *   - 半径（XZ）≈ 1.1835 单位
 *   - 总高度（Y）≈ 4.2514 单位
 *
 * 为使渲染网格与 Jolt 物理胶囊体对齐，各 Prefab 的 Transform::scale 按下列公式推导：
 *   - scale_XZ = phys_radius   / mesh_radius   (= 0.5 / 1.1835)
 *   - scale_Y  = phys_total_h  / mesh_total_h  (= (2*halfH + 2*r) / 4.2514)
 *
 * 正确做法是将 Capsule.obj 在 DCC 工具中归一化后重新导出，届时缩放可恢复为 (1,1,1)。
 * 在归一化完成之前，每个使用 Capsule.obj 的 Prefab 必须按其各自的物理参数单独计算缩放，
 * 不得在不同物理尺寸的 Prefab 之间共用同一套缩放值。
 */
#include "PrefabFactory.h"

#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_T_MainCamera.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_Material.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Components/C_D_TriMeshCollider.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_InvisibleWall.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_D_EnemyDormant.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_T_DeathZone.h"
#include "Game/Components/C_T_Enemy.h"
#include "Game/Components/C_T_ItemPickup.h"
#include "Game/Components/C_D_AIState.h"
#include "Game/Components/C_D_AIPerception.h"
#include "Game/Components/C_D_NavAgent.h"
#include "Game/Components/C_T_Pathfinder.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Components/C_T_TriggerZone.h"
#include "Game/Components/C_T_FinishZone.h"
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

    // C_D_Material（默认 BlinnPhong）
    reg.Emplace<C_D_Material>(entity);

    // C_D_DebugName（规范必选）
    AttachDebugName(reg, entity, "ENTITY_Env_Floor_Main");

    LOG_INFO("[PrefabFactory] CreateFloor id=" << entity);

    return entity;
}

// ============================================================
// CreateStaticMap  →  PREFAB_ENV_TUTORIAL_MAP
// ============================================================
/**
 * @brief 创建静态地图渲染实体（通用，适用于所有场景地图）。
 *
 * 仅添加渲染组件（C_D_MeshRenderer + C_D_Transform），不创建物理碰撞体。
 * 物理支撑由 CreateNavMeshFloor 的三角网格地板提供。
 *
 * @param reg      ECS Registry
 * @param mapMesh  地图网格句柄（由 AssetManager::LoadMesh 获取）
 * @param scale    地图缩放系数（TutorialLevel=2.0，其余场景=1.0）
 * @return 创建的实体 ID
 */
EntityID PrefabFactory::CreateStaticMap(Registry& reg, ECS::MeshHandle mapMesh, float scale)
{
    EntityID entity = reg.Create();

    // C_D_Transform（Y=-6*scale 与 NavTest 坐标系对齐）
    reg.Emplace<C_D_Transform>(entity,
        Vector3(0.0f, -6.0f * scale, 0.0f),
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(scale, scale, scale)
    );

    // C_D_MeshRenderer（使用 TutorialMap.obj，.mtl 由 Assimp 自动加载）
    reg.Emplace<C_D_MeshRenderer>(entity,
        mapMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_RigidBody（静态体）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Box，覆盖地图全局足迹，随 scale 等比扩展）
    C_D_Collider col{};
    col.type   = ColliderType::Box;
    col.half_x = 25.0f * scale;
    col.half_y =  0.6f * scale;
    col.half_z = 25.0f * scale;
    reg.Emplace<C_D_Collider>(entity, col);

    // C_D_Material（默认 BlinnPhong）
    reg.Emplace<C_D_Material>(entity);

    // C_D_DebugName（规范必选）
    AttachDebugName(reg, entity, "ENTITY_Env_TutorialMap");

    LOG_INFO("[PrefabFactory] CreateStaticMap id=" << entity);

    return entity;
}

// ============================================================
// CreateStaticMapRenderOnly  →  PREFAB_ENV_MAP_RENDER_ONLY
// ============================================================
/**
 * @brief 创建纯渲染地图实体（碰撞由 NavMeshFloor 承担）。
 *
 * 不挂载任何物理组件（RigidBody / Collider），碰撞完全由独立的
 * NavMeshFloor TriMesh 实体提供。实现渲染 mesh 与碰撞 mesh 完全分离。
 */
EntityID PrefabFactory::CreateStaticMapRenderOnly(Registry& reg, ECS::MeshHandle mapMesh, float scale)
{
    EntityID entity = reg.Create();

    // C_D_Transform（Y=-6*scale 与 NavTest 坐标系对齐）
    reg.Emplace<C_D_Transform>(entity,
        Vector3(0.0f, -6.0f * scale, 0.0f),
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(scale, scale, scale)
    );

    // C_D_MeshRenderer（渲染用，.mtl 由 Assimp 自动加载）
    reg.Emplace<C_D_MeshRenderer>(entity,
        mapMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_Material（默认 BlinnPhong）
    reg.Emplace<C_D_Material>(entity);

    // 不挂载 C_D_RigidBody / C_D_Collider — 碰撞由 NavMeshFloor 独立提供

    AttachDebugName(reg, entity, "ENTITY_Env_MapRenderOnly");

    LOG_INFO("[PrefabFactory] CreateStaticMapRenderOnly id=" << entity
             << " scale=" << scale);

    return entity;
}

// ============================================================
// CreateFinishZoneMesh  →  PREFAB_ENV_FINISH_ZONE_MESH
// ============================================================
/**
 * @brief 创建可见终点区域（渲染 + TriMesh Trigger 碰撞）。
 *
 * 渲染 mesh 与碰撞 mesh 分离：
 *   - 渲染使用 renderMesh（OBJ 模型）
 *   - 碰撞使用独立 TriMesh 三角网格（可来自不同 OBJ 或 navmesh 数据）
 * TriMesh 碰撞设为 Trigger 模式，不产生物理推力，仅触发事件。
 */
EntityID PrefabFactory::CreateFinishZoneMesh(
    Registry&                               reg,
    ECS::MeshHandle                         renderMesh,
    const std::vector<NCL::Maths::Vector3>& collisionVerts,
    const std::vector<int>&                 collisionIndices,
    Vector3                                 worldOffset,
    float                                   scale)
{
    if (collisionVerts.empty() || collisionIndices.empty() || collisionIndices.size() % 3 != 0) {
        LOG_WARN("[PrefabFactory] CreateFinishZoneMesh: invalid collision geometry (verts="
                 << collisionVerts.size() << " idx=" << collisionIndices.size() << "), skipping.");
        return ECS::Entity::NULL_ENTITY;
    }

    EntityID entity = reg.Create();

    // C_D_Transform
    reg.Emplace<C_D_Transform>(entity,
        worldOffset,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(scale, scale, scale)
    );

    // C_D_MeshRenderer（可见终点区域）
    reg.Emplace<C_D_MeshRenderer>(entity,
        renderMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_RigidBody（静态体）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_TriMeshCollider（三角网格触发器）
    C_D_TriMeshCollider tri{};
    tri.vertices    = collisionVerts;
    tri.indices     = collisionIndices;
    tri.friction    = 0.0f;
    tri.restitution = 0.0f;
    tri.is_trigger  = true;
    reg.Emplace<C_D_TriMeshCollider>(entity, std::move(tri));

    // C_T_TriggerZone + C_T_FinishZone 标签（供 Sys_LevelGoal 检测终点到达）
    reg.Emplace<C_T_TriggerZone>(entity);
    reg.Emplace<C_T_FinishZone>(entity);

    // C_D_Material（红色基础颜色，匹配 TutorialMap_finish.mtl 的 Kd 1 0 0）
    C_D_Material mat{};
    mat.baseColour = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    reg.Emplace<C_D_Material>(entity, mat);

    AttachDebugName(reg, entity, "ENTITY_Env_FinishZone");

    LOG_INFO("[PrefabFactory] CreateFinishZoneMesh id=" << entity
             << " verts=" << collisionVerts.size()
             << " tris=" << (collisionIndices.size() / 3)
             << " offset=(" << worldOffset.x << "," << worldOffset.y << "," << worldOffset.z << ")");

    return entity;
}

// ============================================================
// CreatePlayer  →  PREFAB_PLAYER
// ============================================================
EntityID PrefabFactory::CreatePlayer(
    Registry&       reg,
    ECS::MeshHandle cubeMesh,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    // C_D_Transform
    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_MeshRenderer（暂用 cube，后续替换为角色模型）
    reg.Emplace<ECS::C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    // C_D_RigidBody（动态体，高线性阻尼辅助限速，锁定全轴旋转防翻滚）
    C_D_RigidBody rb{};
    rb.mass            = 5.0f;
    rb.gravity_factor  = 1.0f;
    rb.linear_damping  = 0.5f;
    rb.angular_damping = 0.05f;
    rb.lock_rotation_x = true;
    rb.lock_rotation_y = true;
    rb.lock_rotation_z = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Capsule：半径 0.5，半高 1.0）
    C_D_Collider col{};
    col.type        = ColliderType::Capsule;
    col.half_x      = 0.5f;   // radius
    col.half_y      = 1.0f;   // half height
    col.friction    = 0.5f;
    col.restitution = 0.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    // C_T_Player 标签
    reg.Emplace<ECS::C_T_Player>(entity);

    // C_D_PlayerState（MGS 风格潜行状态）
    reg.Emplace<ECS::C_D_PlayerState>(entity, ECS::C_D_PlayerState{});

    // C_D_Input（输入数据，由 Sys_InputDispatch 每帧写入）
    reg.Emplace<ECS::C_D_Input>(entity, ECS::C_D_Input{});

    // C_D_CQCState（CQC 近身制服状态）
    reg.Emplace<ECS::C_D_CQCState>(entity, ECS::C_D_CQCState{});

    // C_D_Health（生命值）
    reg.Emplace<ECS::C_D_Health>(entity, ECS::C_D_Health{});

    // C_D_DebugName
    AttachDebugName(reg, entity, "ENTITY_Player_Main");

    LOG_INFO("[PrefabFactory] CreatePlayer id=" << entity
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreateInvisibleWall  →  PREFAB_ENV_INVISIBLE_WALL
// ============================================================
EntityID PrefabFactory::CreateInvisibleWall(
    Registry&   reg,
    int         wallIndex,
    Vector3     position,
    Vector3     halfExtents,
    Quaternion  rotation)
{
    EntityID entity = reg.Create();

    // C_D_Transform（位置/旋转由参数指定，scale 固定 1）
    reg.Emplace<C_D_Transform>(entity,
        position,
        rotation,
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_RigidBody（静态体）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Box，无摩擦无弹性 → 纯阻挡）
    C_D_Collider col{};
    col.type        = ColliderType::Box;
    col.half_x      = halfExtents.x;
    col.half_y      = halfExtents.y;
    col.half_z      = halfExtents.z;
    col.friction    = 0.0f;
    col.restitution = 0.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    // C_T_InvisibleWall 标签
    reg.Emplace<ECS::C_T_InvisibleWall>(entity);

    // 不挂载 C_D_MeshRenderer → 渲染不可见

    // C_D_DebugName（含序号，匹配 ENTITY_Env_InvisibleWall_XX 规范）
    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Env_InvisibleWall_%02d", wallIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreateInvisibleWall id=" << entity
             << " index=" << wallIndex
             << " pos=(" << position.x << "," << position.y << "," << position.z
             << ") half=(" << halfExtents.x << "," << halfExtents.y << "," << halfExtents.z << ")");

    return entity;
}

// ============================================================
// CreateTriggerZone  →  PREFAB_TRIGGER_ZONE
// ============================================================
/**
 * @brief 创建静态 Trigger 区域实体（PREFAB_TRIGGER_ZONE）的实现。
 *
 * @details
 * 该 Prefab 仅用于重叠检测，不产生物理响应，也不挂载渲染组件。
 * 物理层面使用静态 Box Sensor，并通过 `C_T_TriggerZone` 标签供游戏逻辑识别。
 */
EntityID PrefabFactory::CreateTriggerZone(
    Registry&   reg,
    Vector3     position,
    Vector3     halfExtents)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type       = ColliderType::Box;
    col.half_x     = halfExtents.x;
    col.half_y     = halfExtents.y;
    col.half_z     = halfExtents.z;
    col.is_trigger = true;
    reg.Emplace<C_D_Collider>(entity, col);

    reg.Emplace<C_T_TriggerZone>(entity);
    AttachDebugName(reg, entity, "ENTITY_Trigger_Zone");

    LOG_INFO("[PrefabFactory] CreateTriggerZone id=" << entity
             << " pos=(" << position.x << "," << position.y << "," << position.z
             << ") half=(" << halfExtents.x << "," << halfExtents.y << "," << halfExtents.z << ")");

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
    rb.mass             = 1.0f;
    rb.gravity_factor   = 1.0f;
    rb.linear_damping   = 0.05f;
    rb.angular_damping  = 0.05f;
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

    // C_D_Material（默认 BlinnPhong）
    reg.Emplace<C_D_Material>(entity);

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
    reg.Emplace<C_D_EnemyDormant>(entity, C_D_EnemyDormant{});

    auto& detect = reg.Emplace<C_D_AIPerception>(entity);
    detect.detection_value              = 0.0f;
    detect.detection_value_increase     = 15.0f;
    detect.detection_value_decrease     = 5.0f;

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Enemy_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreatePhysicsEnemy id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreateNavEnemy  →  PREFAB_NAV_ENEMY (来自 feat/navtest-scene)
// ============================================================
/**
 * @brief 创建带 NavAgent 的导航敌人实体（PREFAB_NAV_ENEMY）的实现。
 *
 * @details
 * **渲染缩放对齐（Capsule.obj → 物理胶囊体）：**
 *
 * Capsule.obj 原始尺寸：半径 ≈ 1.1835，总高 ≈ 4.2514。
 * 本 Prefab 的物理碰撞体参数：radius=0.5，halfHeight=1.0，
 * 对应 Jolt 胶囊总高度 = 2×1.0 + 2×0.5 = 3.0。
 *
 * 推导：
 *   scale_XZ = 0.5  / 1.1835 ≈ 0.4225
 *   scale_Y  = 3.0  / 4.2514 ≈ 0.7058
 *
 * 注意：NavEnemy 与 CreatePhysicsCapsule 的物理尺寸不同（halfHeight 1.0 vs 0.5），
 * 因此两者的 Y 缩放必须各自独立计算，不可共用。
 */
EntityID PrefabFactory::CreateNavEnemy(
    Registry&       reg,
    ECS::MeshHandle enemyMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    // cube.obj 顶点 ±1.0，scale (1,1,1) → 世界尺寸 2×2×2，与 Box 碰撞体 half(1,1,1) 匹配
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
    col.type   = ColliderType::Box;
    col.half_x = 1.0f;
    col.half_y = 1.0f;
    col.half_z = 1.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    // EnemyAI 核心组件（状态机）
    reg.Emplace<C_T_Enemy>(entity);
    reg.Emplace<C_D_AIState>(entity);
    reg.Emplace<C_D_EnemyDormant>(entity, C_D_EnemyDormant{});

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
// CreateNavTarget  →  PREFAB_NAV_TARGET (来自 feat/navtest-scene)
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

// ============================================================
// CreateDeathZone  →  PREFAB_ENV_DEATH_ZONE
// ============================================================
EntityID PrefabFactory::CreateDeathZone(
    Registry&   reg,
    int         zoneIndex,
    Vector3     position,
    Vector3     halfExtents)
{
    EntityID entity = reg.Create();

    // C_D_Transform（位置由参数指定，scale 固定 1）
    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_RigidBody（静态体）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_Collider（Box 触发器，不产生物理响应）
    C_D_Collider col{};
    col.type        = ColliderType::Box;
    col.half_x      = halfExtents.x;
    col.half_y      = halfExtents.y;
    col.half_z      = halfExtents.z;
    col.friction    = 0.0f;
    col.restitution = 0.0f;
    col.is_trigger  = true;
    reg.Emplace<C_D_Collider>(entity, col);

    // C_T_DeathZone 标签
    reg.Emplace<C_T_DeathZone>(entity);

    // 不挂载 C_D_MeshRenderer → 渲染不可见

    // C_D_DebugName
    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Env_DeathZone_%02d", zoneIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreateDeathZone id=" << entity
             << " index=" << zoneIndex
             << " pos=(" << position.x << "," << position.y << "," << position.z
             << ") half=(" << halfExtents.x << "," << halfExtents.y << "," << halfExtents.z << ")");

    return entity;
}

// ============================================================
// CreatePhysicsCapsule  →  PREFAB_PHYSICS_CAPSULE (来自 master)
// ============================================================
/**
 * @brief 创建动态物理胶囊体实体（PREFAB_PHYSICS_CAPSULE）的实现。
 *
 * @details
 * **渲染缩放对齐（Capsule.obj → 物理胶囊体）：**
 *
 * Capsule.obj 原始尺寸：半径 ≈ 1.1835，总高 ≈ 4.2514。
 * 本 Prefab 的物理碰撞体参数：radius=0.5，halfHeight=0.5，
 * 对应 Jolt 胶囊总高度 = 2×0.5 + 2×0.5 = 2.0。
 *
 * 推导：
 *   scale_XZ = 0.5  / 1.1835 ≈ 0.4225
 *   scale_Y  = 2.0  / 4.2514 ≈ 0.4704
 *
 * **锁轴说明：** lock_rotation_x/z 防止胶囊在碰撞中侧翻，保持竖直姿态。
 * **阻尼说明：** linear/angular_damping=0.05 提供轻微稳定性，避免无限滑行。
 */
EntityID PrefabFactory::CreatePhysicsCapsule(
    Registry&       reg,
    ECS::MeshHandle capsuleMesh,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    // 缩放推导见文件头注释与函数 Doxygen
    // scale_XZ = phys_radius(0.5) / mesh_radius(1.1835) ≈ 0.4225
    // scale_Y  = phys_total_h(2.0) / mesh_total_h(4.2514) ≈ 0.4704
    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(0.4225f, 0.4704f, 0.4225f)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        capsuleMesh,
        static_cast<uint32_t>(0)
    );

    C_D_RigidBody rb{};
    rb.mass            = 1.0f;
    rb.gravity_factor  = 1.0f;
    rb.linear_damping  = 0.05f;
    rb.angular_damping = 0.05f;
    rb.lock_rotation_x = true;
    rb.lock_rotation_z = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // 物理胶囊总高度 2.0：2 * half_height(0.5) + 2 * radius(0.5) = 2.0
    C_D_Collider col{};
    col.type        = ColliderType::Capsule;
    col.half_x      = 0.5f;   // radius
    col.half_y      = 0.5f;   // half_height（不含半球部分）
    col.friction    = 0.5f;
    col.restitution = 0.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Physics_Capsule_%02d", spawnIndex);
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreatePhysicsCapsule id=" << entity
             << " index=" << spawnIndex
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreateNavMeshFloor  →  PREFAB_ENV_NAVMESH_FLOOR
// ============================================================
/**
 * @brief 从 NavMesh 可行走三角形创建静态 TriMesh 地板碰撞实体。
 *
 * 顶点坐标为 NavMesh 本地空间（ScaleVertices 后），worldOffset 将其平移到
 * 世界坐标（通常为 (0, -6*scale, 0)）。
 * 若 vertices/indices 为空或 indices 不是 3 的倍数，直接返回 NULL_ENTITY。
 *
 * @param reg        ECS Registry
 * @param vertices   NavMesh 可行走顶点（来自 GetWalkableGeometry）
 * @param indices    三角形索引（每 3 个构成一个三角形）
 * @param worldOffset 世界空间偏移（对齐地图渲染位置）
 * @return 创建的实体 ID，或 NULL_ENTITY（几何体无效时）
 */
ECS::EntityID PrefabFactory::CreateNavMeshFloor(
    ECS::Registry&                          reg,
    const std::vector<NCL::Maths::Vector3>& vertices,
    const std::vector<int>&                 indices,
    NCL::Maths::Vector3                     worldOffset)
{
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
        LOG_WARN("[PrefabFactory] CreateNavMeshFloor: invalid geometry (verts="
                 << vertices.size() << " idx=" << indices.size() << "), skipping.");
        return ECS::Entity::NULL_ENTITY;
    }

    EntityID entity = reg.Create();

    // C_D_Transform：体原点设为 worldOffset，顶点为该原点的局部空间
    reg.Emplace<C_D_Transform>(entity,
        worldOffset,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    // C_D_RigidBody（静态体）
    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    // C_D_TriMeshCollider（三角网格地板）
    C_D_TriMeshCollider tri{};
    tri.vertices    = vertices;
    tri.indices     = indices;
    tri.friction    = 0.5f;
    tri.restitution = 0.0f;
    reg.Emplace<C_D_TriMeshCollider>(entity, std::move(tri));

    // C_D_DebugName
    AttachDebugName(reg, entity, "ENTITY_Env_NavMeshFloor");

    LOG_INFO("[PrefabFactory] CreateNavMeshFloor id=" << entity
             << " verts=" << vertices.size()
             << " tris=" << (indices.size() / 3)
             << " offset=(" << worldOffset.x << "," << worldOffset.y << "," << worldOffset.z << ")");

    return entity;
}
