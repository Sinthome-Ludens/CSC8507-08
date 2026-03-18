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
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Components/C_D_HoloBaitState.h"
#include "Game/Components/C_T_RoamAI.h"
#include "Game/Components/C_D_RoamAI.h"
#include "Game/Components/C_T_KeyCard.h"
#include "Game/Components/C_D_DoorLocked.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/PrefabLoader.h"

#include <cstring>
#include <cstdio>
#include <cmath>

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
    /*
     * Load JSON defaults from Prefab_Camera_Main.json.
     * Function parameters (position, pitch, yaw) override the loaded defaults.
     */
    PrefabLoader::PrefabCameraDefaults defs;
    PrefabLoader::LoadCameraDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    C_D_Camera cam  = defs.camera;
    cam.pitch       = pitch;
    cam.yaw         = yaw;
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
    /*
     * Load JSON defaults from Prefab_Env_Floor.json.
     * Position, scale, RigidBody, and Collider all come from the loaded defaults.
     */
    PrefabLoader::PrefabFloorDefaults defs;
    PrefabLoader::LoadFloorDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        defs.position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        defs.scale
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

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
// CreateStaticMapEntity  →  PREFAB_ENV_STATIC_MAP
// ============================================================
/**
 * @brief Single-entity static map: render mesh + TriMesh collision in one entity.
 *
 * Collision vertices must be pre-scaled and winding-corrected by the caller.
 * The entity's Transform uses worldOffset for position, scale for uniform scaling.
 * Render mesh uses *.obj (correct face normals), collision uses _collision.obj geometry.
 */
EntityID PrefabFactory::CreateStaticMapEntity(
    Registry&                               reg,
    ECS::MeshHandle                         renderMesh,
    const std::vector<NCL::Maths::Vector3>& collVerts,
    const std::vector<int>&                 collIndices,
    Vector3                                 worldOffset,
    float                                   scale)
{
    if (collVerts.empty() || collIndices.empty() || collIndices.size() % 3 != 0) {
        LOG_WARN("[PrefabFactory] CreateStaticMapEntity: invalid collision geometry (verts="
                 << collVerts.size() << " idx=" << collIndices.size() << "), skipping.");
        return ECS::Entity::NULL_ENTITY;
    }

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        worldOffset,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(scale, scale, scale)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        renderMesh,
        static_cast<uint32_t>(0)
    );

    reg.Emplace<C_D_Material>(entity);

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type        = ColliderType::TriMesh;
    col.triVerts    = collVerts;
    col.triIndices  = collIndices;
    col.friction    = 0.5f;
    col.restitution = 0.0f;
    reg.Emplace<C_D_Collider>(entity, std::move(col));

    AttachDebugName(reg, entity, "ENTITY_Env_StaticMap");

    LOG_INFO("[PrefabFactory] CreateStaticMapEntity id=" << entity
             << " verts=" << collVerts.size()
             << " tris=" << (collIndices.size() / 3)
             << " scale=" << scale);

    return entity;
}

// ============================================================
// CreateStaticMapRenderOnly  →  PREFAB_ENV_MAP_RENDER_ONLY (deprecated)
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

    // C_D_Collider（TriMesh 三角网格触发器）
    C_D_Collider col{};
    col.type        = ColliderType::TriMesh;
    col.triVerts    = collisionVerts;
    col.triIndices  = collisionIndices;
    col.friction    = 0.0f;
    col.restitution = 0.0f;
    col.is_trigger  = true;
    reg.Emplace<C_D_Collider>(entity, std::move(col));

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
// CreateFinishZoneRender  →  PREFAB_ENV_FINISH_ZONE_RENDER
// ============================================================
/**
 * @brief Render-only finish zone entity (red material, no collider).
 *
 * Placed at worldOffset with uniform scale; the OBJ vertices carry the
 * local-space position of the finish area.
 */
EntityID PrefabFactory::CreateFinishZoneRender(
    Registry&       reg,
    ECS::MeshHandle finishMesh,
    Vector3         worldOffset,
    float           scale)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        worldOffset,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(scale, scale, scale));

    reg.Emplace<C_D_MeshRenderer>(entity,
        finishMesh, static_cast<uint32_t>(0));

    C_D_Material mat{};
    mat.baseColour = Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    reg.Emplace<C_D_Material>(entity, mat);

    AttachDebugName(reg, entity, "ENTITY_Env_FinishZoneRender");

    LOG_INFO("[PrefabFactory] CreateFinishZoneRender id=" << entity);
    return entity;
}

// ============================================================
// CreateFinishZoneDetect  →  PREFAB_ENV_FINISH_ZONE_DETECT
// ============================================================
/**
 * @brief Invisible detection entity at the OBJ geometric center (world space).
 *
 * Sys_LevelGoal uses distance to this entity's C_T_FinishZone tag to determine
 * whether the player has reached the goal.
 */
EntityID PrefabFactory::CreateFinishZoneDetect(
    Registry& reg,
    Vector3   detectPos)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        detectPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f));

    reg.Emplace<C_T_FinishZone>(entity);

    AttachDebugName(reg, entity, "ENTITY_Env_FinishZoneDetect");

    LOG_INFO("[PrefabFactory] CreateFinishZoneDetect id=" << entity
             << " pos=(" << detectPos.x << "," << detectPos.y << "," << detectPos.z << ")");
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
    /*
     * Load JSON defaults from Prefab_Player.json.
     * spawnPos from function parameter overrides transform position.
     * RigidBody, Collider, and Health values come from the loaded defaults.
     */
    PrefabLoader::PrefabPlayerDefaults defs;
    PrefabLoader::LoadPlayerDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    reg.Emplace<ECS::C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

    reg.Emplace<ECS::C_T_Player>(entity);

    reg.Emplace<ECS::C_D_PlayerState>(entity, ECS::C_D_PlayerState{});

    reg.Emplace<ECS::C_D_Input>(entity, ECS::C_D_Input{});

    reg.Emplace<ECS::C_D_CQCState>(entity, ECS::C_D_CQCState{});

    ECS::C_D_Health health{};
    health.hp    = defs.hp;
    health.maxHp = defs.maxHp;
    reg.Emplace<ECS::C_D_Health>(entity, health);

    reg.Emplace<C_T_NavTarget>(entity);

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
    /*
     * Load JSON defaults from Prefab_Env_InvisibleWall.json.
     * Position, halfExtents, rotation come from function parameters.
     * Friction and restitution come from the loaded defaults.
     */
    PrefabLoader::PrefabInvisibleWallDefaults defs;
    PrefabLoader::LoadInvisibleWallDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        rotation,
        Vector3(1.0f, 1.0f, 1.0f)
    );

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col   = defs.col;
    col.half_x         = halfExtents.x;
    col.half_y         = halfExtents.y;
    col.half_z         = halfExtents.z;
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
    /*
     * Load JSON defaults from Prefab_TriggerZone.json.
     * Position and halfExtents come from function parameters.
     * is_trigger flag comes from the loaded defaults.
     */
    PrefabLoader::PrefabTriggerZoneDefaults defs;
    PrefabLoader::LoadTriggerZoneDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col = defs.col;
    col.half_x       = halfExtents.x;
    col.half_y       = halfExtents.y;
    col.half_z       = halfExtents.z;
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
    /*
     * Load JSON defaults from Prefab_Physics_Cube.json.
     * spawnPos from function parameter overrides transform position.
     * RigidBody and Collider come from the loaded defaults.
     */
    PrefabLoader::PrefabPhysicsCubeDefaults defs;
    PrefabLoader::LoadPhysicsCubeDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

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
    /*
     * Load JSON defaults from Prefab_Physics_Enemy.json.
     * spawnPos from function parameter overrides transform position.
     * RigidBody, Collider, and perception rates come from the loaded defaults.
     */
    PrefabLoader::PrefabEnemyDefaults defs;
    PrefabLoader::LoadPhysicsEnemyDefaults(defs);

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

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

    reg.Emplace<C_T_Enemy>(entity);
    reg.Emplace<C_D_AIState>(entity);
    reg.Emplace<C_D_EnemyDormant>(entity, C_D_EnemyDormant{});

    auto& detect = reg.Emplace<C_D_AIPerception>(entity);
    detect.detection_value              = 0.0f;
    detect.detection_value_increase     = defs.detection_increase;
    detect.detection_value_decrease     = defs.detection_decrease;

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
    /*
     * Load JSON defaults from Prefab_Nav_Enemy.json.
     * spawnPos from function parameter overrides transform position.
     * RigidBody, Collider, and perception rates come from the loaded defaults.
     */
    PrefabLoader::PrefabEnemyDefaults defs;
    PrefabLoader::LoadNavEnemyDefaults(defs);

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

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

    reg.Emplace<C_T_Enemy>(entity);
    reg.Emplace<C_D_AIState>(entity);
    reg.Emplace<C_D_EnemyDormant>(entity, C_D_EnemyDormant{});

    auto& detect = reg.Emplace<C_D_AIPerception>(entity);
    detect.detection_value          = 0.0f;
    detect.detection_value_increase = defs.detection_increase;
    detect.detection_value_decrease = defs.detection_decrease;

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
    /*
     * Load JSON defaults from Prefab_Nav_Target.json.
     * spawnPos from function parameter overrides transform position.
     * Scale and Collider come from the loaded defaults.
     */
    PrefabLoader::PrefabNavTargetDefaults defs;
    PrefabLoader::LoadNavTargetDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        defs.scale
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        targetMesh,
        static_cast<uint32_t>(0)
    );

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

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
// AttachPatrolRoute — patrol waypoints + initial facing
// ============================================================
/**
 * @brief Attaches a C_D_PatrolRoute component to an existing enemy entity and
 * rotates the entity to face the second waypoint (avoids facing a wall on spawn).
 */
void PrefabFactory::AttachPatrolRoute(
    Registry&      reg,
    EntityID       entity,
    const Vector3* waypoints,
    int            count,
    Vector3        spawnPos)
{
    if (count < 2) return;

    auto& patrol = reg.Emplace<C_D_PatrolRoute>(entity);
    patrol.count = std::min(count, ECS::PATROL_MAX_WAYPOINTS);
    for (int p = 0; p < patrol.count; ++p) {
        patrol.waypoints[p] = waypoints[p];
    }
    patrol.current_index = 0;
    patrol.needs_path    = true;

    Vector3 dir = patrol.waypoints[1] - spawnPos;
    dir.y = 0.0f;
    float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
    if (len > 0.01f) {
        float yaw = atan2f(-dir.x / len, -dir.z / len) * 57.29577f;
        auto& tf = reg.Get<C_D_Transform>(entity);
        tf.rotation = Quaternion::EulerAnglesToQuaternion(0, yaw, 0);
    }

    LOG_INFO("[PrefabFactory] AttachPatrolRoute entity=" << entity
             << " waypoints=" << patrol.count);
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
    /*
     * Load JSON defaults from Prefab_Env_DeathZone.json.
     * Position and halfExtents come from function parameters.
     * Friction, restitution, and is_trigger come from the loaded defaults.
     */
    PrefabLoader::PrefabDeathZoneDefaults defs;
    PrefabLoader::LoadDeathZoneDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col = defs.col;
    col.half_x       = halfExtents.x;
    col.half_y       = halfExtents.y;
    col.half_z       = halfExtents.z;
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
    /*
     * Load JSON defaults from Prefab_Physics_Capsule.json.
     * spawnPos from function parameter overrides transform position.
     * Scale, RigidBody, and Collider come from the loaded defaults.
     */
    PrefabLoader::PrefabPhysicsCapsuleDefaults defs;
    PrefabLoader::LoadPhysicsCapsuleDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        defs.scale
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        capsuleMesh,
        static_cast<uint32_t>(0)
    );

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    reg.Emplace<C_D_Collider>(entity, defs.col);

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

    // C_D_Collider（TriMesh 三角网格地板）
    C_D_Collider col{};
    col.type        = ColliderType::TriMesh;
    col.triVerts    = vertices;
    col.triIndices  = indices;
    col.friction    = 0.5f;
    col.restitution = 0.0f;
    reg.Emplace<C_D_Collider>(entity, std::move(col));

    // C_D_DebugName
    AttachDebugName(reg, entity, "ENTITY_Env_NavMeshFloor");

    LOG_INFO("[PrefabFactory] CreateNavMeshFloor id=" << entity
             << " verts=" << vertices.size()
             << " tris=" << (indices.size() / 3)
             << " offset=(" << worldOffset.x << "," << worldOffset.y << "," << worldOffset.z << ")");

    return entity;
}

// ============================================================
// CreateHoloBait  →  PREFAB_HOLO_BAIT
// ============================================================

EntityID PrefabFactory::CreateHoloBait(
    Registry& reg,
    Vector3   worldPos)
{
    /*
     * Load JSON defaults from Prefab_HoloBait.json.
     * worldPos from function parameter overrides transform position.
     * Scale and remainingTime come from the loaded defaults.
     */
    PrefabLoader::PrefabHoloBaitDefaults defs;
    PrefabLoader::LoadHoloBaitDefaults(defs);

    EntityID entity = reg.Create();

    auto& tf = reg.Emplace<C_D_Transform>(entity);
    tf.position = worldPos;
    tf.scale    = defs.scale;

    auto& bait = reg.Emplace<C_D_HoloBaitState>(entity);
    bait.worldPos      = worldPos;
    bait.remainingTime = defs.remainingTime;
    bait.active        = true;

    AttachDebugName(reg, entity, "ENTITY_HoloBait");

    LOG_INFO("[PrefabFactory] CreateHoloBait id=" << entity
             << " pos=(" << worldPos.x << "," << worldPos.y << "," << worldPos.z << ")");

    return entity;
}

// ============================================================
// CreateRoamAI  →  PREFAB_ROAM_AI
// ============================================================

EntityID PrefabFactory::CreateRoamAI(
    Registry& reg,
    Vector3   targetPos)
{
    /*
     * Load JSON defaults from Prefab_RoamAI.json.
     * targetPos from function parameter overrides transform position.
     * Scale, roamSpeed, waypointInterval, and detectRadius come from the loaded defaults.
     */
    PrefabLoader::PrefabRoamAIDefaults defs;
    PrefabLoader::LoadRoamAIDefaults(defs);

    EntityID entity = reg.Create();

    auto& tf = reg.Emplace<C_D_Transform>(entity);
    tf.position = targetPos + Vector3(0.0f, 0.5f, 0.0f);
    tf.scale    = defs.scale;

    reg.Emplace<C_T_RoamAI>(entity);

    auto& roam = reg.Emplace<C_D_RoamAI>(entity);
    roam.targetPos        = targetPos;
    roam.roamSpeed        = defs.roamSpeed;
    roam.waypointInterval = defs.waypointInterval;
    roam.detectRadius     = defs.detectRadius;
    roam.active           = true;

    AttachDebugName(reg, entity, "ENTITY_RoamAI");

    LOG_INFO("[PrefabFactory] CreateRoamAI id=" << entity
             << " target=(" << targetPos.x << "," << targetPos.y << "," << targetPos.z << ")");

    return entity;
}

// ============================================================
// CreateKeyCard  →  PREFAB_KEY_CARD
// ============================================================
/**
 * @brief Create a collectible key card entity (PREFAB_KEY_CARD).
 *
 * Attaches: C_D_Transform (0.5 scale), C_D_MeshRenderer, C_D_Material (yellow),
 *           C_T_KeyCard, C_D_DebugName.
 *
 * @param reg       ECS Registry.
 * @param cubeMesh  Cube mesh handle used for rendering.
 * @param keyId     Key identifier (must match a door's requiredKeyId to unlock it).
 * @param position  World-space spawn position.
 * @return Created entity ID.
 */
EntityID PrefabFactory::CreateKeyCard(
    Registry&       reg,
    ECS::MeshHandle cubeMesh,
    uint8_t         keyId,
    Vector3         position)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(0.5f, 0.5f, 0.5f)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    C_D_Material mat{};
    mat.baseColour = Vector4(1.0f, 1.0f, 0.0f, 1.0f); // yellow key card
    reg.Emplace<C_D_Material>(entity, mat);

    reg.Emplace<C_T_KeyCard>(entity, C_T_KeyCard{ keyId });

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_KeyCard_%02d", static_cast<int>(keyId));
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreateKeyCard id=" << entity
             << " keyId=" << (int)keyId
             << " pos=(" << position.x << "," << position.y << "," << position.z << ")");

    return entity;
}

// ============================================================
// CreateLockedDoor  →  PREFAB_LOCKED_DOOR
// ============================================================
/**
 * @brief Create a static locked door entity (PREFAB_LOCKED_DOOR).
 *
 * Attaches: C_D_Transform (scale = halfExtents×2), C_D_MeshRenderer,
 *           C_D_Material (brown), C_D_RigidBody (static), C_D_Collider (Box),
 *           C_D_DoorLocked, C_D_DebugName.
 *
 * @param reg         ECS Registry.
 * @param cubeMesh    Cube mesh handle used for rendering.
 * @param keyId       Required key identifier to unlock this door.
 * @param position    World-space centre position.
 * @param halfExtents Box collider half-extents (already scaled by caller).
 * @return Created entity ID.
 */
EntityID PrefabFactory::CreateLockedDoor(
    Registry&       reg,
    ECS::MeshHandle cubeMesh,
    uint8_t         keyId,
    Vector3         position,
    Vector3         halfExtents)
{
    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(halfExtents.x * 2.0f, halfExtents.y * 2.0f, halfExtents.z * 2.0f)
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    C_D_Material mat{};
    mat.baseColour = Vector4(0.6f, 0.3f, 0.1f, 1.0f); // brown door
    reg.Emplace<C_D_Material>(entity, mat);

    C_D_RigidBody rb{};
    rb.is_static = true;
    reg.Emplace<C_D_RigidBody>(entity, rb);

    C_D_Collider col{};
    col.type        = ColliderType::Box;
    col.half_x      = halfExtents.x;
    col.half_y      = halfExtents.y;
    col.half_z      = halfExtents.z;
    col.friction    = 0.5f;
    col.restitution = 0.0f;
    reg.Emplace<C_D_Collider>(entity, col);

    reg.Emplace<C_D_DoorLocked>(entity, C_D_DoorLocked{ keyId });

    char debugName[64];
    std::snprintf(debugName, sizeof(debugName), "ENTITY_Door_%02d", static_cast<int>(keyId));
    AttachDebugName(reg, entity, debugName);

    LOG_INFO("[PrefabFactory] CreateLockedDoor id=" << entity
             << " keyId=" << (int)keyId
             << " pos=(" << position.x << "," << position.y << "," << position.z
             << ") half=(" << halfExtents.x << "," << halfExtents.y << "," << halfExtents.z << ")");

    return entity;
}
