/**
 * @file PrefabFactory.cpp
 * @brief 实体预制体工厂实现：通用数据驱动 Create + 向后兼容 thin wrapper
 *
 * @details
 * ## 架构
 *
 * 核心入口 Create() / CreateVariant() 通过 ComponentRegistry 反射表从 JSON 蓝图
 * 创建实体。全部 21 个旧 Create* 方法保留为 thin wrapper，内部构造 RuntimeOverrides
 * 后调用通用入口，保证调用方零改动。
 *
 * ## 胶囊体渲染缩放说明
 *
 * 项目所用的 Capsule.obj（来自 Assets/Meshes/）原始尺寸未归一化：
 *   - 半径（XZ）≈ 1.1835 单位
 *   - 总高度（Y）≈ 4.2514 单位
 *
 * 为使渲染网格与 Jolt 物理胶囊体对齐，各 Prefab 的 Transform::scale 按下列公式推导：
 *   - scale_XZ = phys_radius   / mesh_radius   (= 0.5 / 1.1835)
 *   - scale_Y  = phys_total_h  / mesh_total_h  (= (2*halfH + 2*r) / 4.2514)
 */
#include "PrefabFactory.h"
#include "ComponentRegistry.h"

#include "Assets.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_Material.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_T_ItemPickup.h"
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Components/C_D_HoloBaitState.h"
#include "Game/Components/C_T_RoamAI.h"
#include "Game/Components/C_D_RoamAI.h"
#include "Game/Components/C_T_KeyCard.h"
#include "Game/Components/C_D_DoorLocked.h"
#include "Game/Utils/Log.h"
#include "Game/Utils/PrefabLoader.h"

#include <nlohmann/json.hpp>
#include <cstring>
#include <cstdio>
#include <cmath>

using json = nlohmann::json;
using namespace NCL::Maths;
using namespace ECS;

/**
 * @brief 从 JSON 根对象提取 "Components" 子对象。
 * @param root JSON 文档根对象
 * @return 指向 "Components" JSON 对象的 const 指针；若不存在或不是 object 返回 nullptr
 */
static const json* GetComponents(const json& root) {
    if (!root.contains("Components") || !root["Components"].is_object()) return nullptr;
    return &root["Components"];
}

/**
 * @brief 遍历 JSON Components 字典，按 key 查找 ComponentRegistry 并逐一 Emplace。
 *
 * 对于未注册的组件 key，输出 LOG_WARN 并跳过（不阻断其余组件的创建）。
 * overrides 的优先级：RuntimeOverrides 字段 > JSON 字段 > 组件默认构造值。
 *
 * @param reg       ECS Registry
 * @param comps     JSON "Components" 字典对象
 * @param overrides 运行时覆盖参数
 * @return 新创建的实体 ID（始终有效，即使部分组件跳过）
 */
static EntityID EmplaceFromJson(
    Registry& reg,
    const json& comps,
    const RuntimeOverrides& overrides)
{
    EntityID entity = reg.Create();
    for (auto it = comps.begin(); it != comps.end(); ++it) {
        const EmplaceFn* fn = ComponentRegistry::Find(it.key());
        if (!fn) {
            LOG_WARN("[PrefabFactory] Unknown component: " << it.key());
            continue;
        }
        (*fn)(reg, entity, it.value(), overrides);
    }
    return entity;
}

/**
 * @brief 通用数据驱动创建入口：从 JSON 蓝图的 "Components" 字典创建实体。
 *
 * 首次调用时触发 ComponentRegistry::RegisterAll()（幂等）。
 * 优先级链：RuntimeOverrides > JSON 字段 > 组件默认值。
 *
 * @param reg             ECS Registry
 * @param prefabJsonFile  JSON 蓝图文件名（如 "Prefab_Player.json"）
 * @param overrides       运行时覆盖参数
 * @return 创建的实体 ID；JSON 文件不存在或缺少 "Components" 时返回 Entity::NULL_ENTITY
 */
EntityID PrefabFactory::Create(
    Registry&               reg,
    const std::string&      prefabJsonFile,
    const RuntimeOverrides& overrides)
{
    ComponentRegistry::RegisterAll();

    const json* doc = PrefabLoader::LoadBlueprint(prefabJsonFile);
    if (!doc) return Entity::NULL_ENTITY;

    const json* comps = GetComponents(*doc);
    if (!comps) return Entity::NULL_ENTITY;

    return EmplaceFromJson(reg, *comps, overrides);
}

/**
 * @brief 从 JSON 蓝图的 "Variants" 中创建指定变体实体。
 *
 * 查找 doc["Variants"][variantName]["Components"]，然后走与 Create() 相同的
 * EmplaceFromJson 路径。用于同一 JSON 文件中声明多个变体的场景（如 FinishZone
 * 的 Mesh/Render/Detect 三变体）。
 *
 * @param reg             ECS Registry
 * @param prefabJsonFile  JSON 蓝图文件名
 * @param variantName     变体名称（如 "Mesh", "Render", "Detect"）
 * @param overrides       运行时覆盖参数
 * @return 创建的实体 ID；变体不存在时返回 Entity::NULL_ENTITY
 */
EntityID PrefabFactory::CreateVariant(
    Registry&               reg,
    const std::string&      prefabJsonFile,
    const std::string&      variantName,
    const RuntimeOverrides& overrides)
{
    ComponentRegistry::RegisterAll();

    const json* doc = PrefabLoader::LoadBlueprint(prefabJsonFile);
    if (!doc || !doc->contains("Variants")) return Entity::NULL_ENTITY;

    auto& variants = (*doc)["Variants"];
    if (!variants.contains(variantName)) return Entity::NULL_ENTITY;

    auto& variant = variants[variantName];
    if (!variant.contains("Components") || !variant["Components"].is_object())
        return Entity::NULL_ENTITY;

    return EmplaceFromJson(reg, variant["Components"], overrides);
}

// ============================================================
// CreateCameraMain — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateCameraMain(
    Registry&   reg,
    Vector3     position,
    float       pitch,
    float       yaw)
{
    RuntimeOverrides ovr;
    ovr.position = position;
    EntityID id = Create(reg, "Prefab_Camera_Main.json", ovr);
    if (id != Entity::NULL_ENTITY) {
        auto& cam = reg.Get<C_D_Camera>(id);
        cam.pitch = pitch;
        cam.yaw   = yaw;
    }
    return id;
}

// ============================================================
// CreateFloor — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateFloor(Registry& reg, MeshHandle cubeMesh)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = cubeMesh;
    return Create(reg, "Prefab_Env_Floor.json", ovr);
}

// ============================================================
// CreateStaticMap — thin wrapper (含计算逻辑)
// ============================================================
EntityID PrefabFactory::CreateStaticMap(Registry& reg, MeshHandle mapMesh, float scale)
{
    RuntimeOverrides ovr;
    ovr.meshHandle  = mapMesh;
    ovr.scale       = Vector3(scale, scale, scale);
    ovr.worldOffset = Vector3(0.0f, -6.0f * scale, 0.0f);
    ovr.halfExtents = Vector3(25.0f * scale, 0.6f * scale, 25.0f * scale);
    return Create(reg, "Prefab_Env_StaticMapBox.json", ovr);
}

// ============================================================
// CreateStaticMapEntity — thin wrapper (TriMesh 碰撞几何)
// ============================================================
EntityID PrefabFactory::CreateStaticMapEntity(
    Registry&                               reg,
    MeshHandle                              renderMesh,
    const std::vector<NCL::Maths::Vector3>& collVerts,
    const std::vector<int>&                 collIndices,
    Vector3                                 worldOffset,
    float                                   scale)
{
    if (collVerts.empty() || collIndices.empty() || collIndices.size() % 3 != 0) {
        LOG_WARN("[PrefabFactory] CreateStaticMapEntity: invalid collision geometry (verts="
                 << collVerts.size() << " idx=" << collIndices.size() << "), skipping.");
        return Entity::NULL_ENTITY;
    }

    RuntimeOverrides ovr;
    ovr.meshHandle  = renderMesh;
    ovr.worldOffset = worldOffset;
    ovr.scale       = Vector3(scale, scale, scale);
    ovr.triVerts    = &collVerts;
    ovr.triIndices  = &collIndices;
    return Create(reg, "Prefab_Env_StaticMap.json", ovr);
}

// ============================================================
// CreateStaticMapRenderOnly — thin wrapper (纯渲染，无碰撞)
// ============================================================
EntityID PrefabFactory::CreateStaticMapRenderOnly(Registry& reg, MeshHandle mapMesh, float scale)
{
    RuntimeOverrides ovr;
    ovr.meshHandle  = mapMesh;
    ovr.scale       = Vector3(scale, scale, scale);
    ovr.worldOffset = Vector3(0.0f, -6.0f * scale, 0.0f);
    return Create(reg, "Prefab_Env_MapRenderOnly.json", ovr);
}

// ============================================================
// CreateFinishZoneMesh — thin wrapper (TriMesh Trigger)
// ============================================================
EntityID PrefabFactory::CreateFinishZoneMesh(
    Registry&                               reg,
    MeshHandle                              renderMesh,
    const std::vector<NCL::Maths::Vector3>& collisionVerts,
    const std::vector<int>&                 collisionIndices,
    Vector3                                 worldOffset,
    float                                   scale)
{
    if (collisionVerts.empty() || collisionIndices.empty() || collisionIndices.size() % 3 != 0) {
        LOG_WARN("[PrefabFactory] CreateFinishZoneMesh: invalid collision geometry (verts="
                 << collisionVerts.size() << " idx=" << collisionIndices.size() << "), skipping.");
        return Entity::NULL_ENTITY;
    }

    RuntimeOverrides ovr;
    ovr.meshHandle  = renderMesh;
    ovr.worldOffset = worldOffset;
    ovr.scale       = Vector3(scale, scale, scale);
    ovr.triVerts    = &collisionVerts;
    ovr.triIndices  = &collisionIndices;
    return CreateVariant(reg, "Prefab_Env_FinishZone.json", "Mesh", ovr);
}

// ============================================================
// CreateFinishZoneRender — thin wrapper (纯视觉，无碰撞)
// ============================================================
EntityID PrefabFactory::CreateFinishZoneRender(
    Registry&   reg,
    MeshHandle  finishMesh,
    Vector3     worldOffset,
    float       scale)
{
    RuntimeOverrides ovr;
    ovr.meshHandle  = finishMesh;
    ovr.worldOffset = worldOffset;
    ovr.scale       = Vector3(scale, scale, scale);
    return CreateVariant(reg, "Prefab_Env_FinishZone.json", "Render", ovr);
}

// ============================================================
// CreateFinishZoneDetect — thin wrapper (不可见，仅检测)
// ============================================================
EntityID PrefabFactory::CreateFinishZoneDetect(
    Registry& reg,
    Vector3   detectPos)
{
    RuntimeOverrides ovr;
    ovr.position = detectPos;
    return CreateVariant(reg, "Prefab_Env_FinishZone.json", "Detect", ovr);
}

// ============================================================
// CreatePlayer — thin wrapper
// ============================================================
EntityID PrefabFactory::CreatePlayer(
    Registry&   reg,
    MeshHandle  cubeMesh,
    Vector3     spawnPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = cubeMesh;
    ovr.position   = spawnPos;
    EntityID entity = Create(reg, "Prefab_Player.json", ovr);
    if (entity == Entity::NULL_ENTITY) {
        return entity;
    }

    if (reg.Has<C_D_Collider>(entity)) {
        auto& col = reg.Get<C_D_Collider>(entity);
        col.type = ColliderType::Box;
        col.fit_mode = ColliderFitMode::MeshBoundsAuto;
        col.fit_padding = 0.0f;
    }

    if (reg.Has<C_D_PlayerState>(entity)) {
        auto& ps = reg.Get<C_D_PlayerState>(entity);
        ps.colliderRadius = 1.0f;
        ps.colliderHalfHeight = 1.0f;
    }

    return entity;
}

static void AttachDebugName(Registry& reg, EntityID entity, const char* name)
{
    auto& dn = reg.Emplace<C_D_DebugName>(entity);
    std::snprintf(dn.name, sizeof(dn.name), "%s", name);
}

// ============================================================
// CreateInvisibleWall — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateInvisibleWall(
    Registry&   reg,
    int         wallIndex,
    Vector3     position,
    Vector3     halfExtents,
    Quaternion  rotation)
{
    RuntimeOverrides ovr;
    ovr.position    = position;
    ovr.rotation    = rotation;
    ovr.halfExtents = halfExtents;
    ovr.spawnIndex  = wallIndex;
    return Create(reg, "Prefab_Env_InvisibleWall.json", ovr);
}

// ============================================================
// CreateTriggerZone — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateTriggerZone(
    Registry&   reg,
    Vector3     position,
    Vector3     halfExtents)
{
    RuntimeOverrides ovr;
    ovr.position    = position;
    ovr.halfExtents = halfExtents;
    return Create(reg, "Prefab_TriggerZone.json", ovr);
}

// ============================================================
// CreatePhysicsCube — thin wrapper
// ============================================================
EntityID PrefabFactory::CreatePhysicsCube(
    Registry&   reg,
    MeshHandle  cubeMesh,
    int         spawnIndex,
    Vector3     spawnPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = cubeMesh;
    ovr.spawnIndex = spawnIndex;
    ovr.position   = spawnPos;
    return Create(reg, "Prefab_Physics_Cube.json", ovr);
}

// ============================================================
// CreatePhysicsEnemy — thin wrapper
// ============================================================
EntityID PrefabFactory::CreatePhysicsEnemy(
    Registry&   reg,
    MeshHandle  enemyMesh,
    int         spawnIndex,
    Vector3     spawnPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = enemyMesh;
    ovr.spawnIndex = spawnIndex;
    ovr.position   = spawnPos;
    return Create(reg, "Prefab_Physics_Enemy.json", ovr);
}

// ============================================================
// CreateNavEnemy — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateNavEnemy(
    Registry&   reg,
    MeshHandle  enemyMesh,
    int         spawnIndex,
    Vector3     spawnPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = enemyMesh;
    ovr.spawnIndex = spawnIndex;
    ovr.position   = spawnPos;
    return Create(reg, "Prefab_Nav_Enemy.json", ovr);
}

// ============================================================
// CreateNavTarget — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateNavTarget(
    Registry&   reg,
    MeshHandle  targetMesh,
    int         spawnIndex,
    Vector3     spawnPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = targetMesh;
    ovr.spawnIndex = spawnIndex;
    ovr.position   = spawnPos;
    return Create(reg, "Prefab_Nav_Target.json", ovr);
}

// ============================================================
// AttachPatrolRoute — 保持不变（后创建修改器，非 prefab 创建）
// ============================================================
void PrefabFactory::AttachPatrolRoute(
    Registry&      reg,
    EntityID       entity,
    const Vector3* waypoints,
    int            count,
    Vector3        spawnPos)
{
    if (count < 2) return;

    auto& patrol = reg.Emplace<C_D_PatrolRoute>(entity);
    patrol.count = std::min(count, PATROL_MAX_WAYPOINTS);
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
// CreateDeathZone — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateDeathZone(
    Registry&   reg,
    int         zoneIndex,
    Vector3     position,
    Vector3     halfExtents)
{
    RuntimeOverrides ovr;
    ovr.position    = position;
    ovr.halfExtents = halfExtents;
    ovr.spawnIndex  = zoneIndex;
    return Create(reg, "Prefab_Env_DeathZone.json", ovr);
}

// ============================================================
// CreatePhysicsCapsule — thin wrapper
// ============================================================
EntityID PrefabFactory::CreatePhysicsCapsule(
    Registry&   reg,
    MeshHandle  capsuleMesh,
    int         spawnIndex,
    Vector3     spawnPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = capsuleMesh;
    ovr.spawnIndex = spawnIndex;
    ovr.position   = spawnPos;
    return Create(reg, "Prefab_Physics_Capsule.json", ovr);
}

// ============================================================
// CreateNavMeshFloor — thin wrapper (TriMesh 碰撞几何)
// ============================================================
EntityID PrefabFactory::CreateNavMeshFloor(
    Registry&                               reg,
    const std::vector<NCL::Maths::Vector3>& vertices,
    const std::vector<int>&                 indices,
    Vector3                                 worldOffset)
{
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
        LOG_WARN("[PrefabFactory] CreateNavMeshFloor: invalid geometry (verts="
                 << vertices.size() << " idx=" << indices.size() << "), skipping.");
        return Entity::NULL_ENTITY;
    }

    RuntimeOverrides ovr;
    ovr.worldOffset = worldOffset;
    ovr.triVerts    = &vertices;
    ovr.triIndices  = &indices;
    return Create(reg, "Prefab_Env_NavMeshFloor.json", ovr);
}

// ============================================================
// CreateItemPickup  →  PREFAB_ITEM_PICKUP
// ============================================================
EntityID PrefabFactory::CreateItemPickup(
    Registry&       reg,
    ECS::MeshHandle cubeMesh,
    ECS::ItemID     itemId,
    uint8_t         quantity,
    int             spawnIndex,
    Vector3         spawnPos)
{
    EntityID entity = reg.Create();

    // Weapons use capsule mesh, gadgets use cube mesh
    bool isWeapon = (GetItemType(itemId) == ItemType::Weapon);
    ECS::MeshHandle actualMesh = cubeMesh;
    Vector3 pickupScale(0.7f, 0.7f, 0.7f);
    if (isWeapon) {
        actualMesh = ECS::AssetManager::Instance().LoadMesh(
            NCL::Assets::MESHDIR + "Capsule.obj");
        pickupScale = Vector3(0.5f, 0.5f, 0.5f);
    }

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        pickupScale
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        actualMesh,
        static_cast<uint32_t>(0)
    );

    // Per-item color
    C_D_Material mat{};
    switch (itemId) {
        case ItemID::HoloBait:     mat.baseColour = Vector4(0.95f, 0.85f, 0.1f, 1.0f);  break; // yellow
        case ItemID::PhotonRadar:  mat.baseColour = Vector4(0.1f, 0.85f, 0.95f, 1.0f);  break; // cyan
        case ItemID::DDoS:         mat.baseColour = Vector4(0.7f, 0.2f, 0.9f, 1.0f);    break; // purple
        case ItemID::RoamAI:       mat.baseColour = Vector4(0.2f, 0.85f, 0.2f, 1.0f);   break; // green
        case ItemID::TargetStrike: mat.baseColour = Vector4(0.9f, 0.2f, 0.2f, 1.0f);    break; // red
        case ItemID::GlobalMap:    mat.baseColour = Vector4(0.2f, 0.6f, 0.95f, 1.0f);   break; // blue
        default:                   mat.baseColour = Vector4(0.8f, 0.8f, 0.8f, 1.0f);    break; // gray
    }
    reg.Emplace<C_D_Material>(entity, mat);

    auto& pickup = reg.Emplace<C_T_ItemPickup>(entity);
    pickup.itemId   = itemId;
    pickup.quantity = quantity;

    auto& dn = reg.Emplace<C_D_DebugName>(entity);
    std::snprintf(dn.name, sizeof(dn.name), "ENTITY_ItemPickup_%02d", spawnIndex);

    LOG_INFO("[PrefabFactory] CreateItemPickup id=" << entity
             << " itemId=" << static_cast<int>(itemId)
             << " qty=" << (int)quantity
             << " pos=(" << spawnPos.x << "," << spawnPos.y << "," << spawnPos.z << ")");

    return entity;
}

// ============================================================
// CreateHoloBait — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateHoloBait(
    Registry& reg,
    Vector3   worldPos)
{
    RuntimeOverrides ovr;
    ovr.position  = worldPos;
    ovr.targetPos = worldPos;  // C_D_HoloBaitState.worldPos uses targetPos
    return Create(reg, "Prefab_HoloBait.json", ovr);
}

// ============================================================
// CreateRoamAI — thin wrapper (Y+0.5 偏移 + targetPos)
// ============================================================
EntityID PrefabFactory::CreateRoamAI(
    Registry& reg,
    Vector3   targetPos)
{
    RuntimeOverrides ovr;
    ovr.position  = targetPos + Vector3(0.0f, 0.5f, 0.0f);
    ovr.targetPos = targetPos;
    return Create(reg, "Prefab_RoamAI.json", ovr);
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
    /*
     * Load JSON defaults from Prefab_KeyCard.json.
     * position and keyId from function parameters override the loaded defaults.
     * Scale and baseColour come from the loaded defaults.
     */
    PrefabLoader::PrefabKeyCardDefaults defs;
    PrefabLoader::LoadKeyCardDefaults(defs);

    EntityID entity = reg.Create();

    reg.Emplace<C_D_Transform>(entity,
        position,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        defs.scale
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        cubeMesh,
        static_cast<uint32_t>(0)
    );

    C_D_Material mat{};
    mat.baseColour = defs.baseColour;
    reg.Emplace<C_D_Material>(entity, mat);

    reg.Emplace<C_T_KeyCard>(entity, C_T_KeyCard{ keyId });

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
    /*
     * Load JSON defaults from Prefab_LockedDoor.json.
     * position, keyId, halfExtents from function parameters override the loaded defaults.
     * RigidBody, Collider friction/restitution, and baseColour come from the loaded defaults.
     */
    PrefabLoader::PrefabLockedDoorDefaults defs;
    PrefabLoader::LoadLockedDoorDefaults(defs);

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
    mat.baseColour = defs.baseColour;
    reg.Emplace<C_D_Material>(entity, mat);

    reg.Emplace<C_D_RigidBody>(entity, defs.rb);

    C_D_Collider col = defs.col;
    col.half_x       = halfExtents.x;
    col.half_y       = halfExtents.y;
    col.half_z       = halfExtents.z;
    reg.Emplace<C_D_Collider>(entity, col);

    reg.Emplace<C_D_DoorLocked>(entity, C_D_DoorLocked{ keyId });

    LOG_INFO("[PrefabFactory] CreateLockedDoor id=" << entity
             << " keyId=" << (int)keyId
             << " pos=(" << position.x << "," << position.y << "," << position.z
             << ") half=(" << halfExtents.x << "," << halfExtents.y << "," << halfExtents.z << ")");

    return entity;
}
