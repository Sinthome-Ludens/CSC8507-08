/**
 * @file PrefabLoader.cpp
 * @brief JSON Prefab 加载工具实现：缓存、解析、字段提取。
 *
 * @details
 * 内部维护一个 filename->json 缓存，避免同一场景内多次重复读取磁盘。
 * 每个 ReadXxx 辅助函数使用 json.contains() 进行安全字段读取，
 * 缺失字段不报错，保留调用方提供的默认值。
 *
 * 所有文件路径基于 NCL::Assets::ASSETROOT + "Prefabs/"。
 */
#include "PrefabLoader.h"
#include "Game/Utils/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>
#include <unordered_map>

#include "Assets.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

using json = nlohmann::json;
using namespace NCL::Maths;

namespace ECS {
namespace PrefabLoader {

/* ================================================================
 * 静态缓存
 * ================================================================ */
static std::unordered_map<std::string, json> s_cache;

/* ================================================================
 * PrefabDir — 返回 Prefab 目录完整路径
 * ================================================================ */
const std::string& PrefabDir() {
    static std::string dir = NCL::Assets::ASSETROOT + "Prefabs/";
    return dir;
}

/* ================================================================
 * ClearCache — 清除 JSON 文件缓存
 * ================================================================ */
void ClearCache() {
    s_cache.clear();
    LOG_INFO("[PrefabLoader] Cache cleared.");
}

/* ================================================================
 * LoadJSON — 内部辅助，读取并缓存 JSON 文件
 * ================================================================ */
static const json* LoadJSON(const std::string& filename) {
    auto it = s_cache.find(filename);
    if (it != s_cache.end()) {
        return &it->second;
    }

    std::string fullPath = PrefabDir() + filename;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        LOG_WARN("[PrefabLoader] Cannot open file: " << fullPath);
        return nullptr;
    }

    try {
        json doc = json::parse(file);
        auto result = s_cache.emplace(filename, std::move(doc));
        return &result.first->second;
    }
    catch (const json::exception& e) {
        LOG_ERROR("[PrefabLoader] JSON parse error in " << filename << ": " << e.what());
        return nullptr;
    }
}

/* ================================================================
 * ReadVec3 — 从 JSON 数组 [x, y, z] 读取 Vector3
 * ================================================================ */
static void ReadVec3(const json& j, const char* key, Vector3& out) {
    if (!j.contains(key)) return;
    auto& arr = j[key];
    if (!arr.is_array() || arr.size() < 3) return;
    out.x = arr[0].get<float>();
    out.y = arr[1].get<float>();
    out.z = arr[2].get<float>();
}

/* ================================================================
 * ReadVec4 — 从 JSON 数组 [x, y, z, w] 读取 Vector4
 * ================================================================ */
static void ReadVec4(const json& j, const char* key, Vector4& out) {
    if (!j.contains(key)) return;
    auto& arr = j[key];
    if (!arr.is_array() || arr.size() < 4) return;
    out.x = arr[0].get<float>();
    out.y = arr[1].get<float>();
    out.z = arr[2].get<float>();
    out.w = arr[3].get<float>();
}

/* ================================================================
 * ReadRigidBody — 从 "C_D_RigidBody" JSON 对象填充 C_D_RigidBody
 * ================================================================ */
static void ReadRigidBody(const json& j, C_D_RigidBody& rb) {
    if (j.contains("mass"))            rb.mass            = j["mass"].get<float>();
    if (j.contains("linear_damping"))  rb.linear_damping  = j["linear_damping"].get<float>();
    if (j.contains("angular_damping")) rb.angular_damping = j["angular_damping"].get<float>();
    if (j.contains("gravity_factor"))  rb.gravity_factor  = j["gravity_factor"].get<float>();
    if (j.contains("is_static"))       rb.is_static       = j["is_static"].get<bool>();
    if (j.contains("is_kinematic"))    rb.is_kinematic    = j["is_kinematic"].get<bool>();
    if (j.contains("lock_rotation_x")) rb.lock_rotation_x = j["lock_rotation_x"].get<bool>();
    if (j.contains("lock_rotation_y")) rb.lock_rotation_y = j["lock_rotation_y"].get<bool>();
    if (j.contains("lock_rotation_z")) rb.lock_rotation_z = j["lock_rotation_z"].get<bool>();
}

/* ================================================================
 * ReadCollider — 从 "C_D_Collider" JSON 对象填充 C_D_Collider
 *
 * JSON 中 Capsule 使用 "radius" / "half_height" 字段名，
 * 映射到 C_D_Collider 的 half_x(radius) / half_y(half_height)。
 * Box/Sphere 使用 "half_x", "half_y", "half_z"。
 * ================================================================ */
static void ReadCollider(const json& j, C_D_Collider& col) {
    if (j.contains("type")) {
        std::string t = j["type"].get<std::string>();
        if      (t == "Box")     col.type = ColliderType::Box;
        else if (t == "Sphere")  col.type = ColliderType::Sphere;
        else if (t == "Capsule") col.type = ColliderType::Capsule;
        else if (t == "TriMesh") col.type = ColliderType::TriMesh;
    }

    if (col.type == ColliderType::Capsule) {
        if (j.contains("radius"))      col.half_x = j["radius"].get<float>();
        if (j.contains("half_height")) col.half_y = j["half_height"].get<float>();
    }
    else {
        if (j.contains("half_x")) col.half_x = j["half_x"].get<float>();
        if (j.contains("half_y")) col.half_y = j["half_y"].get<float>();
        if (j.contains("half_z")) col.half_z = j["half_z"].get<float>();
    }

    if (j.contains("friction"))    col.friction    = j["friction"].get<float>();
    if (j.contains("restitution")) col.restitution = j["restitution"].get<float>();
    if (j.contains("is_trigger"))  col.is_trigger  = j["is_trigger"].get<bool>();
}

/* ================================================================
 * ReadCamera — 从 "C_D_Camera" JSON 对象填充 C_D_Camera
 * ================================================================ */
static void ReadCamera(const json& j, C_D_Camera& cam) {
    if (j.contains("fov"))         cam.fov         = j["fov"].get<float>();
    if (j.contains("near_z"))      cam.near_z      = j["near_z"].get<float>();
    if (j.contains("far_z"))       cam.far_z       = j["far_z"].get<float>();
    if (j.contains("pitch"))       cam.pitch       = j["pitch"].get<float>();
    if (j.contains("yaw"))         cam.yaw         = j["yaw"].get<float>();
    if (j.contains("move_speed"))  cam.move_speed  = j["move_speed"].get<float>();
    if (j.contains("sensitivity")) cam.sensitivity = j["sensitivity"].get<float>();
}

/* ================================================================
 * GetComponents — 辅助：从 JSON 根对象获取 "Components" 子对象
 * ================================================================ */
static const json* GetComponents(const json& root) {
    if (!root.contains("Components")) return nullptr;
    return &root["Components"];
}

/* ================================================================
 * LoadMapConfig
 * ================================================================ */
bool LoadMapConfig(const std::string& prefabName, MapLoadConfig& out) {
    const json* doc = LoadJSON(prefabName);
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (!comps->contains("C_D_MapConfig")) return false;
    auto& mc = (*comps)["C_D_MapConfig"];

    if (mc.contains("renderMesh")) {
        std::string v = mc["renderMesh"].get<std::string>();
        strncpy_s(out.renderMesh, sizeof(out.renderMesh), v.c_str(), sizeof(out.renderMesh) - 1);
    }
    if (mc.contains("collisionMesh")) {
        std::string v = mc["collisionMesh"].get<std::string>();
        strncpy_s(out.collisionMesh, sizeof(out.collisionMesh), v.c_str(), sizeof(out.collisionMesh) - 1);
    }
    if (mc.contains("navmesh")) {
        std::string v = mc["navmesh"].get<std::string>();
        strncpy_s(out.navmesh, sizeof(out.navmesh), v.c_str(), sizeof(out.navmesh) - 1);
    }
    if (mc.contains("finishMesh")) {
        std::string v = mc["finishMesh"].get<std::string>();
        strncpy_s(out.finishMesh, sizeof(out.finishMesh), v.c_str(), sizeof(out.finishMesh) - 1);
    }
    if (mc.contains("startPoints")) {
        std::string v = mc["startPoints"].get<std::string>();
        strncpy_s(out.startPoints, sizeof(out.startPoints), v.c_str(), sizeof(out.startPoints) - 1);
    }
    if (mc.contains("enemySpawns")) {
        std::string v = mc["enemySpawns"].get<std::string>();
        strncpy_s(out.enemySpawns, sizeof(out.enemySpawns), v.c_str(), sizeof(out.enemySpawns) - 1);
    }
    if (mc.contains("mapScale"))     out.mapScale     = mc["mapScale"].get<float>();
    if (mc.contains("yOffset"))      out.yOffset      = mc["yOffset"].get<float>();
    if (mc.contains("flipWinding"))  out.flipWinding  = mc["flipWinding"].get<bool>();

    LOG_INFO("[PrefabLoader] LoadMapConfig from " << prefabName);
    return true;
}

/* ================================================================
 * LoadCameraDefaults — Prefab_Camera_Main.json
 * ================================================================ */
bool LoadCameraDefaults(PrefabCameraDefaults& out) {
    const json* doc = LoadJSON("Prefab_Camera_Main.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Transform")) {
        auto& tf = (*comps)["C_D_Transform"];
        ReadVec3(tf, "position", out.position);
    }

    if (comps->contains("C_D_Camera")) {
        auto& cam = (*comps)["C_D_Camera"];
        ReadCamera(cam, out.camera);
        if (cam.contains("pitch")) out.pitch = cam["pitch"].get<float>();
        if (cam.contains("yaw"))   out.yaw   = cam["yaw"].get<float>();
    }

    LOG_INFO("[PrefabLoader] LoadCameraDefaults OK");
    return true;
}

/* ================================================================
 * LoadFloorDefaults — Prefab_Env_Floor.json
 * ================================================================ */
bool LoadFloorDefaults(PrefabFloorDefaults& out) {
    const json* doc = LoadJSON("Prefab_Env_Floor.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Transform")) {
        auto& tf = (*comps)["C_D_Transform"];
        ReadVec3(tf, "position", out.position);
        ReadVec3(tf, "scale",    out.scale);
    }

    if (comps->contains("C_D_RigidBody")) {
        ReadRigidBody((*comps)["C_D_RigidBody"], out.rb);
    }

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadFloorDefaults OK");
    return true;
}

/* ================================================================
 * LoadPlayerDefaults — Prefab_Player.json
 * ================================================================ */
bool LoadPlayerDefaults(PrefabPlayerDefaults& out) {
    const json* doc = LoadJSON("Prefab_Player.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_RigidBody")) {
        ReadRigidBody((*comps)["C_D_RigidBody"], out.rb);
    }

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    if (comps->contains("C_D_Health")) {
        auto& hp = (*comps)["C_D_Health"];
        if (hp.contains("hp"))    out.hp    = hp["hp"].get<float>();
        if (hp.contains("maxHp")) out.maxHp = hp["maxHp"].get<float>();
    }

    LOG_INFO("[PrefabLoader] LoadPlayerDefaults OK");
    return true;
}

/* ================================================================
 * LoadPhysicsCubeDefaults — Prefab_Physics_Cube.json
 * ================================================================ */
bool LoadPhysicsCubeDefaults(PrefabPhysicsCubeDefaults& out) {
    const json* doc = LoadJSON("Prefab_Physics_Cube.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_RigidBody")) {
        ReadRigidBody((*comps)["C_D_RigidBody"], out.rb);
    }

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadPhysicsCubeDefaults OK");
    return true;
}

/* ================================================================
 * LoadPhysicsCapsuleDefaults — Prefab_Physics_Capsule.json
 * ================================================================ */
bool LoadPhysicsCapsuleDefaults(PrefabPhysicsCapsuleDefaults& out) {
    const json* doc = LoadJSON("Prefab_Physics_Capsule.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Transform")) {
        auto& tf = (*comps)["C_D_Transform"];
        ReadVec3(tf, "scale", out.scale);
    }

    if (comps->contains("C_D_RigidBody")) {
        ReadRigidBody((*comps)["C_D_RigidBody"], out.rb);
    }

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadPhysicsCapsuleDefaults OK");
    return true;
}

/* ================================================================
 * ReadEnemyCommon — 内部辅助，从 Components 读取敌人通用字段
 * ================================================================ */
static void ReadEnemyCommon(const json& comps, PrefabEnemyDefaults& out) {
    if (comps.contains("C_D_RigidBody")) {
        ReadRigidBody(comps["C_D_RigidBody"], out.rb);
    }

    if (comps.contains("C_D_Collider")) {
        ReadCollider(comps["C_D_Collider"], out.col);
    }

    if (comps.contains("C_D_AIPerception")) {
        auto& ai = comps["C_D_AIPerception"];
        if (ai.contains("detection_value_increase")) out.detection_increase = ai["detection_value_increase"].get<float>();
        if (ai.contains("detection_value_decrease")) out.detection_decrease = ai["detection_value_decrease"].get<float>();
    }
}

/* ================================================================
 * LoadPhysicsEnemyDefaults — Prefab_Physics_Enemy.json
 * ================================================================ */
bool LoadPhysicsEnemyDefaults(PrefabEnemyDefaults& out) {
    const json* doc = LoadJSON("Prefab_Physics_Enemy.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    ReadEnemyCommon(*comps, out);

    LOG_INFO("[PrefabLoader] LoadPhysicsEnemyDefaults OK");
    return true;
}

/* ================================================================
 * LoadNavEnemyDefaults — Prefab_Nav_Enemy.json
 * ================================================================ */
bool LoadNavEnemyDefaults(PrefabEnemyDefaults& out) {
    const json* doc = LoadJSON("Prefab_Nav_Enemy.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    ReadEnemyCommon(*comps, out);

    LOG_INFO("[PrefabLoader] LoadNavEnemyDefaults OK");
    return true;
}

/* ================================================================
 * LoadNavTargetDefaults — Prefab_Nav_Target.json
 * ================================================================ */
bool LoadNavTargetDefaults(PrefabNavTargetDefaults& out) {
    const json* doc = LoadJSON("Prefab_Nav_Target.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Transform")) {
        auto& tf = (*comps)["C_D_Transform"];
        ReadVec3(tf, "scale", out.scale);
    }

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadNavTargetDefaults OK");
    return true;
}

/* ================================================================
 * LoadInvisibleWallDefaults — Prefab_Env_InvisibleWall.json
 * ================================================================ */
bool LoadInvisibleWallDefaults(PrefabInvisibleWallDefaults& out) {
    const json* doc = LoadJSON("Prefab_Env_InvisibleWall.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadInvisibleWallDefaults OK");
    return true;
}

/* ================================================================
 * LoadDeathZoneDefaults — Prefab_Env_DeathZone.json
 * ================================================================ */
bool LoadDeathZoneDefaults(PrefabDeathZoneDefaults& out) {
    const json* doc = LoadJSON("Prefab_Env_DeathZone.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadDeathZoneDefaults OK");
    return true;
}

/* ================================================================
 * LoadTriggerZoneDefaults — Prefab_TriggerZone.json
 * ================================================================ */
bool LoadTriggerZoneDefaults(PrefabTriggerZoneDefaults& out) {
    const json* doc = LoadJSON("Prefab_TriggerZone.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Collider")) {
        ReadCollider((*comps)["C_D_Collider"], out.col);
    }

    LOG_INFO("[PrefabLoader] LoadTriggerZoneDefaults OK");
    return true;
}

/* ================================================================
 * LoadHoloBaitDefaults — Prefab_HoloBait.json
 * ================================================================ */
bool LoadHoloBaitDefaults(PrefabHoloBaitDefaults& out) {
    const json* doc = LoadJSON("Prefab_HoloBait.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Transform")) {
        auto& tf = (*comps)["C_D_Transform"];
        ReadVec3(tf, "scale", out.scale);
    }

    if (comps->contains("C_D_HoloBaitState")) {
        auto& bait = (*comps)["C_D_HoloBaitState"];
        if (bait.contains("remainingTime")) out.remainingTime = bait["remainingTime"].get<float>();
    }

    LOG_INFO("[PrefabLoader] LoadHoloBaitDefaults OK");
    return true;
}

/* ================================================================
 * LoadRoamAIDefaults — Prefab_RoamAI.json
 * ================================================================ */
bool LoadRoamAIDefaults(PrefabRoamAIDefaults& out) {
    const json* doc = LoadJSON("Prefab_RoamAI.json");
    if (!doc) return false;

    const json* comps = GetComponents(*doc);
    if (!comps) return false;

    if (comps->contains("C_D_Transform")) {
        auto& tf = (*comps)["C_D_Transform"];
        ReadVec3(tf, "scale", out.scale);
    }

    if (comps->contains("C_D_RoamAI")) {
        auto& roam = (*comps)["C_D_RoamAI"];
        if (roam.contains("roamSpeed"))        out.roamSpeed        = roam["roamSpeed"].get<float>();
        if (roam.contains("waypointInterval")) out.waypointInterval = roam["waypointInterval"].get<float>();
        if (roam.contains("detectRadius"))     out.detectRadius     = roam["detectRadius"].get<float>();
    }

    LOG_INFO("[PrefabLoader] LoadRoamAIDefaults OK");
    return true;
}

} // namespace PrefabLoader
} // namespace ECS

#ifdef _MSC_VER
#pragma warning(pop)
#endif
