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
#include "Game/Components/Res_DataOcean.h"

#include "Assets.h"
#include "Game/Components/C_D_Transform.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Components/C_D_DebugName.h"
#include "Game/Components/C_D_Health.h"
#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_D_InterpBuffer.h"
#include "Game/Components/C_D_MeshRenderer.h"
#include "Game/Components/C_D_Material.h"
#include "Game/Components/C_D_PlayerState.h"
#include "Game/Components/C_D_CQCState.h"
#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_T_ItemPickup.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Components/C_T_NavTarget.h"
#include "Game/Components/C_D_PatrolRoute.h"
#include "Game/Components/C_D_HoloBaitState.h"
#include "Game/Components/C_T_RoamAI.h"
#include "Game/Components/C_D_RoamAI.h"
#include "Game/Components/C_D_Spin.h"
#include "Game/Components/C_T_OrbOfPlayer.h"
#include "Game/Components/C_T_OrbOfEnemy.h"
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

/// 武器拾取物 fallback 模型（无专用 GLTF 视觉时使用）
static const char* kWeaponFallbackMesh = "Capsule.obj";

static AlphaMode ToECSAlphaMode(ImportedAlphaMode mode) {
    switch (mode) {
        case ImportedAlphaMode::Mask:  return AlphaMode::Mask;
        case ImportedAlphaMode::Blend: return AlphaMode::Blend;
        case ImportedAlphaMode::Opaque:
        default:                       return AlphaMode::Opaque;
    }
}

static bool ApplyImportedMeshMaterialDefaults(Registry& reg, EntityID entity, MeshHandle meshHandle) {
    ImportedMaterialDefaults defaults{};
    if (!AssetManager::Instance().GetImportedMaterialDefaults(meshHandle, defaults)) {
        return false;
    }

    C_D_Material material = reg.Has<C_D_Material>(entity)
        ? reg.Get<C_D_Material>(entity)
        : C_D_Material{};

    material.shadingModel = ShadingModel::PBR;
    material.baseColour   = defaults.baseColour;
    material.albedoHandle = defaults.albedoHandle;
    material.normalHandle = defaults.normalHandle;
    material.ormHandle    = defaults.ormHandle;
    material.emissiveHandle = defaults.emissiveHandle;
    material.metallic     = defaults.metallic;
    material.roughness    = defaults.roughness;
    material.ao           = defaults.ao;
    material.alphaCutoff  = defaults.alphaCutoff;
    material.alphaMode    = ToECSAlphaMode(defaults.alphaMode);
    material.doubleSided  = defaults.doubleSided;

    if (reg.Has<C_D_Material>(entity)) {
        reg.Get<C_D_Material>(entity) = material;
    } else {
        reg.Emplace<C_D_Material>(entity, material);
    }
    return true;
}

static MeshHandle ResolveItemVisualMesh(ItemID itemId, MeshHandle fallbackMesh) {
    auto& am = AssetManager::Instance();
    switch (itemId) {
        case ItemID::HoloBait:
            return am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/HoloBait.gltf");
        case ItemID::DDoS:
            return am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/DDOS.gltf");
        case ItemID::RoamAI:
            return am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/RoomAI.gltf");
        case ItemID::TargetStrike:
            return am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/Target.gltf");
        case ItemID::RadarMap:
            return am.LoadMesh(NCL::Assets::ASSETROOT + "GLTF/Orbs/Map.gltf#node=1&recenter=1");
        default:
            return fallbackMesh;
    }
}

static bool HasDedicatedItemVisual(ItemID itemId) {
    switch (itemId) {
        case ItemID::HoloBait:
        case ItemID::DDoS:
        case ItemID::RoamAI:
        case ItemID::TargetStrike:
        case ItemID::RadarMap:
            return true;
        default:
            return false;
    }
}

static Quaternion ResolveItemVisualRotation(ItemID itemId) {
    switch (itemId) {
        case ItemID::RadarMap:
            return Quaternion(-0.4923391342f, -0.5047576427f, 0.5018329024f, 0.5009847283f);
        default:
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

static Vector3 ResolveItemVisualScale(ItemID itemId, bool isWeapon) {
    switch (itemId) {
        case ItemID::RadarMap:
            return Vector3(0.45f, 0.45f, 0.45f);
        default:
            return isWeapon ? Vector3(0.5f, 0.5f, 0.5f)
                            : Vector3(0.7f, 0.7f, 0.7f);
    }
}

static float ResolveItemSpinSpeed(ItemID itemId) {
    switch (itemId) {
        case ItemID::RadarMap:
            return 20.0f;
        default:
            return 0.0f;
    }
}

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
 * @brief 预解析 JSON 蓝图并缓存组件 Emplace 函数指针。
 *
 * 执行一次 JSON 加载 + ComponentRegistry::Find 查表，
 * 将结果缓存到 outCache，供后续 CreateFromCache 零查表调用。
 *
 * @param prefabJsonFile  JSON 蓝图文件名
 * @param outCache        输出缓存列表
 * @return true 成功，false JSON 加载失败或无 Components
 */
bool PrefabFactory::ResolveBlueprintCache(
    const std::string&              prefabJsonFile,
    std::vector<CachedEmplaceEntry>& outCache)
{
    ComponentRegistry::RegisterAll();

    const json* doc = PrefabLoader::LoadBlueprint(prefabJsonFile);
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    outCache.clear();
    outCache.reserve(comps->size());
    for (auto it = comps->begin(); it != comps->end(); ++it) {
        const EmplaceFn* fn = ComponentRegistry::Find(it.key());
        if (!fn) {
            LOG_WARN("[PrefabFactory] ResolveBlueprintCache: unknown component: " << it.key());
            continue;
        }
        CachedEmplaceEntry entry;
        entry.fn       = *fn;
        entry.jsonData = &it.value();
        outCache.push_back(std::move(entry));
    }
    return true;
}

/**
 * @brief 使用预缓存的函数指针创建单个实体，无查表开销。
 *
 * 直接遍历缓存的 {fn, jsonData} 列表调用 Emplace，
 * 跳过 JSON 解析和 ComponentRegistry::Find。
 *
 * @param reg       ECS Registry
 * @param cache     ResolveBlueprintCache 输出的缓存
 * @param overrides 运行时覆盖参数
 * @return 创建的实体 ID
 */
EntityID PrefabFactory::CreateFromCache(
    Registry&                              reg,
    const std::vector<CachedEmplaceEntry>& cache,
    const RuntimeOverrides&                overrides)
{
    EntityID entity = reg.Create();
    for (const auto& entry : cache) {
        entry.fn(reg, entity, *entry.jsonData, overrides);
    }
    return entity;
}

/**
 * @brief Resolve-Once 批量创建：JSON 解析和查表只做一次，循环内直接调用缓存。
 *
 * 1. ComponentRegistry::RegisterAll()（幂等）
 * 2. PrefabLoader::LoadBlueprint（1 次 JSON 加载）
 * 3. 遍历 Components key → ComponentRegistry::Find → 缓存到 vector
 * 4. 循环 overridesList：reg.Create() + 遍历缓存直接调用 fn
 *
 * @param reg             ECS Registry
 * @param prefabJsonFile  JSON 蓝图文件名
 * @param overridesList   每个实体的运行时覆盖参数
 * @return 创建的实体 ID 列表
 */
std::vector<EntityID> PrefabFactory::CreateBatch(
    Registry&                            reg,
    const std::string&                   prefabJsonFile,
    const std::vector<RuntimeOverrides>& overridesList)
{
    std::vector<CachedEmplaceEntry> cache;
    if (!ResolveBlueprintCache(prefabJsonFile, cache)) {
        LOG_WARN("[PrefabFactory] CreateBatch: failed to resolve " << prefabJsonFile);
        return {};
    }

    std::vector<EntityID> result;
    result.reserve(overridesList.size());
    for (const auto& ovr : overridesList) {
        result.push_back(CreateFromCache(reg, cache, ovr));
    }
    return result;
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

/**
 * @brief 通过玩家预制体创建“幽灵玩家”可视实体。
 * @details 先调用 CreatePlayer 复用完整的玩家预制体搭建（模型、层级、调试名等），
 *          保留 Transform / MeshRenderer 等渲染与层级信息，使幽灵与真实玩家在外观与
 *          变换上完全一致。随后在同一函数调用内立即移除一组会参与输入、物理或玩法
 *          逻辑的组件：C_T_Player、C_D_Input、C_D_PlayerState、C_D_CQCState、
 *          C_D_Health、C_T_NavTarget、C_D_RigidBody、C_D_Collider，确保在任意系统
 *          Tick 间隙都不会被当作可驾驶玩家或物理实体参与处理。之后保证存在
 *          C_D_InterpBuffer，用于网络或回放数据驱动的平滑插值。最后覆盖（或补充）
 *          C_D_Material，将材质配置为半透明混合模式、冷色调高亮轮廓与适度自发光，以
 *          视觉上明显区分幽灵与本地实体，同时保持其严格为“只读”的纯可视 ghost 表示。
 */
EntityID PrefabFactory::CreateGhostPlayer(
    Registry&   reg,
    MeshHandle  cubeMesh,
    Vector3     spawnPos)
{
    EntityID entity = CreatePlayer(reg, cubeMesh, spawnPos);
    if (entity == Entity::NULL_ENTITY) {
        return entity;
    }

    if (reg.Has<C_T_Player>(entity)) {
        reg.Remove<C_T_Player>(entity);
    }
    if (reg.Has<C_D_Input>(entity)) {
        reg.Remove<C_D_Input>(entity);
    }
    if (reg.Has<C_D_PlayerState>(entity)) {
        reg.Remove<C_D_PlayerState>(entity);
    }
    if (reg.Has<C_D_CQCState>(entity)) {
        reg.Remove<C_D_CQCState>(entity);
    }
    if (reg.Has<C_D_Health>(entity)) {
        reg.Remove<C_D_Health>(entity);
    }
    if (reg.Has<C_T_NavTarget>(entity)) {
        reg.Remove<C_T_NavTarget>(entity);
    }
    if (reg.Has<C_D_RigidBody>(entity)) {
        reg.Remove<C_D_RigidBody>(entity);
    }
    if (reg.Has<C_D_Collider>(entity)) {
        reg.Remove<C_D_Collider>(entity);
    }

    if (!reg.Has<C_D_InterpBuffer>(entity)) {
        reg.Emplace<C_D_InterpBuffer>(entity);
    }

    if (!reg.Has<C_D_Material>(entity)) {
        reg.Emplace<C_D_Material>(entity);
    }
    auto& material = reg.Get<C_D_Material>(entity);
    material.alphaMode = AlphaMode::Blend;
    material.baseColour = Vector4(0.25f, 0.85f, 1.0f, 0.35f);
    material.emissiveColor = Vector3(0.2f, 0.8f, 1.0f);
    material.emissiveStrength = 0.35f;
    material.rimColour = Vector3(0.6f, 0.95f, 1.0f);
    material.rimPower = 2.5f;
    material.rimStrength = 0.85f;

    if (reg.Has<C_D_DebugName>(entity)) {
        auto& dn = reg.Get<C_D_DebugName>(entity);
        std::snprintf(dn.name, sizeof(dn.name), "ENTITY_Ghost_Player");
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

    bool isWeapon = (GetItemType(itemId) == ItemType::Weapon);
    ECS::MeshHandle actualMesh = INVALID_HANDLE;
    if (HasDedicatedItemVisual(itemId)) {
        actualMesh = ResolveItemVisualMesh(itemId, INVALID_HANDLE);
    }
    if (actualMesh == INVALID_HANDLE) {
        ECS::MeshHandle fallbackMesh = isWeapon
            ? ECS::AssetManager::Instance().LoadMesh(NCL::Assets::MESHDIR + kWeaponFallbackMesh)
            : cubeMesh;
        actualMesh = ResolveItemVisualMesh(itemId, fallbackMesh);
    }
    Vector3 pickupScale = ResolveItemVisualScale(itemId, isWeapon);

    reg.Emplace<C_D_Transform>(entity,
        spawnPos,
        ResolveItemVisualRotation(itemId),
        pickupScale
    );

    reg.Emplace<C_D_MeshRenderer>(entity,
        actualMesh,
        static_cast<uint32_t>(0)
    );

    if (!ApplyImportedMeshMaterialDefaults(reg, entity, actualMesh)) {
        C_D_Material mat{};
        switch (itemId) {
            case ItemID::HoloBait:     mat.baseColour = Vector4(0.95f, 0.85f, 0.1f, 1.0f);  break;
            case ItemID::DDoS:         mat.baseColour = Vector4(0.7f, 0.2f, 0.9f, 1.0f);    break;
            case ItemID::RoamAI:       mat.baseColour = Vector4(0.2f, 0.85f, 0.2f, 1.0f);   break;
            case ItemID::TargetStrike: mat.baseColour = Vector4(0.9f, 0.2f, 0.2f, 1.0f);    break;
            case ItemID::RadarMap:     mat.baseColour = Vector4(0.2f, 0.6f, 0.95f, 1.0f);   break;
            default:                   mat.baseColour = Vector4(0.8f, 0.8f, 0.8f, 1.0f);    break;
        }
        mat.emissiveColor    = { mat.baseColour.x, mat.baseColour.y, mat.baseColour.z };
        mat.emissiveStrength = 2.0f;
        reg.Emplace<C_D_Material>(entity, mat);
    }

    auto& pickup = reg.Emplace<C_T_ItemPickup>(entity);
    pickup.itemId   = itemId;
    pickup.quantity = quantity;

    float spinSpeed = ResolveItemSpinSpeed(itemId);
    if (spinSpeed != 0.0f) {
        C_D_Spin spin{};
        spin.axis = Vector3(0.0f, 1.0f, 0.0f);
        spin.speed = spinSpeed;
        spin.enabled = true;
        reg.Emplace<C_D_Spin>(entity, spin);
    }

    auto& dn = reg.Emplace<C_D_DebugName>(entity);
    std::snprintf(dn.name, sizeof(dn.name), "ENTITY_ItemPickup_%02d", spawnIndex);

    LOG_INFO("[PrefabFactory] CreateItemPickup id=" << entity
             << " itemId=" << static_cast<int>(itemId)
             << " meshHandle=" << actualMesh
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
    EntityID entity = Create(reg, "Prefab_HoloBait.json", ovr);

    MeshHandle holoMesh = AssetManager::Instance().LoadMesh(
        NCL::Assets::ASSETROOT + "GLTF/Orbs/HoloBait.gltf");
    if (reg.Valid(entity) && reg.Has<C_D_MeshRenderer>(entity)) {
        reg.Get<C_D_MeshRenderer>(entity).meshHandle = holoMesh;
        ApplyImportedMeshMaterialDefaults(reg, entity, holoMesh);
    }

    return entity;
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
    EntityID entity = Create(reg, "Prefab_RoamAI.json", ovr);

    MeshHandle roamMesh = AssetManager::Instance().LoadMesh(
        NCL::Assets::ASSETROOT + "GLTF/Orbs/RoomAI.gltf");
    if (reg.Valid(entity) && reg.Has<C_D_MeshRenderer>(entity)) {
        reg.Get<C_D_MeshRenderer>(entity).meshHandle = roamMesh;
        ApplyImportedMeshMaterialDefaults(reg, entity, roamMesh);
    }

    return entity;
}

// ============================================================
// CreateOrbitTriangle — thin wrapper
// ============================================================
EntityID PrefabFactory::CreateOrbitTriangle(
    Registry&   reg,
    MeshHandle  mesh,
    int         spawnIndex,
    Vector3     worldPos)
{
    RuntimeOverrides ovr;
    ovr.meshHandle = mesh;
    ovr.spawnIndex = spawnIndex;
    ovr.position   = worldPos;
    return Create(reg, "Prefab_OrbitTriangle.json", ovr);
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
        Quaternion::AxisAngleToQuaterion(Vector3(0.0f, 0.0f, 1.0f), 90.0f),
        defs.scale
    );

    MeshHandle keyCardMesh = AssetManager::Instance().LoadMesh(
        NCL::Assets::ASSETROOT + "GLTF/Orbs/KeyCard.gltf");

    reg.Emplace<C_D_MeshRenderer>(entity,
        keyCardMesh != INVALID_HANDLE ? keyCardMesh : cubeMesh,
        static_cast<uint32_t>(0)
    );

    if (!ApplyImportedMeshMaterialDefaults(reg, entity, keyCardMesh)) {
        C_D_Material mat{};
        mat.baseColour = defs.baseColour;
        reg.Emplace<C_D_Material>(entity, mat);
    }

    C_D_Spin spin{};
    spin.axis = Vector3(0.0f, 1.0f, 0.0f);
    spin.speed = 18.0f;
    spin.enabled = true;
    reg.Emplace<C_D_Spin>(entity, spin);

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

void PrefabFactory::CreatePlayerOrbs(
    ECS::Registry&  reg,
    ECS::EntityID   playerEntity,
    ECS::MeshHandle innerMesh,
    ECS::MeshHandle outerMesh)
{
    if (playerEntity == ECS::Entity::NULL_ENTITY || !reg.Valid(playerEntity)) {
        return;
    }

    Vector3 spawnPos{0.0f, 0.0f, 0.0f};
    if (reg.Has<C_D_Transform>(playerEntity)) {
        spawnPos = reg.Get<C_D_Transform>(playerEntity).position;
    }

    if (reg.Has<C_D_MeshRenderer>(playerEntity)) {
        reg.Get<C_D_MeshRenderer>(playerEntity).meshHandle = innerMesh;
    }
    ApplyImportedMeshMaterialDefaults(reg, playerEntity, innerMesh);

    EntityID outer = reg.Create();
    reg.Emplace<C_D_Transform>(outer,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );
    reg.Emplace<C_D_MeshRenderer>(outer, outerMesh, static_cast<uint32_t>(0));
    reg.Emplace<C_T_OrbOfPlayer>(outer);

    C_D_Spin spinOut{};
    spinOut.axis = Vector3(1.0f, 1.0f, 0.0f);
    spinOut.speed = 67.5f;
    spinOut.yOffset = 0.0f;
    spinOut.enabled = true;
    reg.Emplace<C_D_Spin>(outer, spinOut);

    ApplyImportedMeshMaterialDefaults(reg, outer, outerMesh);
}

void PrefabFactory::CreateEnemyOrbs(
    ECS::Registry&  reg,
    ECS::EntityID   enemyEntity,
    ECS::MeshHandle innerMesh,
    ECS::MeshHandle outerMesh)
{
    if (enemyEntity == ECS::Entity::NULL_ENTITY || !reg.Valid(enemyEntity)) {
        return;
    }

    Vector3 spawnPos{0.0f, 0.0f, 0.0f};
    if (reg.Has<C_D_Transform>(enemyEntity)) {
        spawnPos = reg.Get<C_D_Transform>(enemyEntity).position;
    }

    if (reg.Has<C_D_MeshRenderer>(enemyEntity)) {
        reg.Get<C_D_MeshRenderer>(enemyEntity).meshHandle = outerMesh;
    }
    ApplyImportedMeshMaterialDefaults(reg, enemyEntity, outerMesh);

    EntityID inner = reg.Create();
    reg.Emplace<C_D_Transform>(inner,
        spawnPos,
        Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
        Vector3(1.0f, 1.0f, 1.0f)
    );
    reg.Emplace<C_D_MeshRenderer>(inner, innerMesh, static_cast<uint32_t>(0));
    reg.Emplace<C_T_OrbOfEnemy>(inner, C_T_OrbOfEnemy{ enemyEntity });

    C_D_Spin spinIn{};
    spinIn.axis = Vector3(0.0f, 1.0f, 0.0f);
    spinIn.speed = 67.5f;
    spinIn.yOffset = 0.0f;
    spinIn.enabled = true;
    reg.Emplace<C_D_Spin>(inner, spinIn);

    ApplyImportedMeshMaterialDefaults(reg, inner, innerMesh);
}
