/**
 * @file ComponentRegistry.cpp
 * @brief 组件反射注册表实现：全部组件类型的 Emplace lambda 注册。
 */
#include "ComponentRegistry.h"

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
#include "Game/Components/C_D_Item.h"
#include "Game/Components/C_D_DataOceanPillar.h"
#include "Game/Utils/PrefabLoader.h"
#include "Game/Utils/Log.h"

#include <nlohmann/json.hpp>
#include <cstring>
#include <cstdio>

using json = nlohmann::json;
using namespace NCL::Maths;

namespace ECS {

/**
 * @brief Meyer's singleton：返回进程级别的 static unordered_map。
 *
 * 首次调用时构造空 map，后续调用返回同一实例。
 * 非线程安全（仅在主线程的 RegisterAll/Find 中调用）。
 */
std::unordered_map<std::string, EmplaceFn>& ComponentRegistry::GetMap() {
    static std::unordered_map<std::string, EmplaceFn> s_map;
    return s_map;
}

/**
 * @brief 注册或覆盖一个组件名到 Emplace 函数的映射。
 *
 * 使用 operator[] 赋值，若 name 已存在则覆盖旧的 EmplaceFn。
 */
void ComponentRegistry::Register(const std::string& name, EmplaceFn fn) {
    GetMap()[name] = std::move(fn);
}

/**
 * @brief 按组件名查找已注册的 Emplace 函数。
 *
 * 返回 map 中元素的指针（进程级生命周期），未找到返回 nullptr。
 * RegisterAll() 完成后 map 不再修改，因此返回的指针稳定。
 */
const EmplaceFn* ComponentRegistry::Find(const std::string& name) {
    auto& map = GetMap();
    auto it = map.find(name);
    return (it != map.end()) ? &it->second : nullptr;
}

/**
 * @brief 幂等注册全部已知组件类型到内部 map。
 *
 * 使用 static bool 保证只执行一次（非线程安全，仅主线程调用）。
 * 注册约 32 个组件的 Emplace lambda，每个 lambda 从 JSON data 读取字段，
 * 并应用 RuntimeOverrides 覆盖，最终调用 Registry::Emplace<T>()。
 */
void ComponentRegistry::RegisterAll() {
    static bool s_registered = false;
    if (s_registered) return;
    s_registered = true;

    // ============================================================
    // C_D_Transform
    // ============================================================
    Register("C_D_Transform", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides& ovr) {
        C_D_Transform tf{};
        PrefabLoader::ReadVec3(data, "position", tf.position);
        PrefabLoader::ReadQuat(data, "rotation", tf.rotation);
        PrefabLoader::ReadVec3(data, "scale",    tf.scale);
        if (ovr.position)    tf.position = *ovr.position;
        if (ovr.worldOffset) tf.position = *ovr.worldOffset;
        if (ovr.rotation)    tf.rotation = *ovr.rotation;
        if (ovr.scale)       tf.scale    = *ovr.scale;
        reg.Emplace<C_D_Transform>(id, tf);
    });

    // ============================================================
    // C_D_MeshRenderer
    // ============================================================
    Register("C_D_MeshRenderer", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides& ovr) {
        uint32_t mesh = 0;
        uint32_t mat  = 0;
        if (ovr.meshHandle) mesh = *ovr.meshHandle;
        if (data.contains("materialHandle") && data["materialHandle"].is_number())
            mat = data["materialHandle"].get<uint32_t>();
        reg.Emplace<C_D_MeshRenderer>(id, mesh, mat);
    });

    // ============================================================
    // C_D_Material
    // ============================================================
    Register("C_D_Material", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        C_D_Material mat{};
        Vector4 col = mat.baseColour;
        PrefabLoader::ReadVec4(data, "baseColour", col);
        mat.baseColour = col;
        reg.Emplace<C_D_Material>(id, mat);
    });

    // ============================================================
    // C_D_RigidBody
    // ============================================================
    Register("C_D_RigidBody", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        C_D_RigidBody rb{};
        PrefabLoader::ReadRigidBody(data, rb);
        reg.Emplace<C_D_RigidBody>(id, rb);
    });

    // ============================================================
    // C_D_Collider
    // ============================================================
    Register("C_D_Collider", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides& ovr) {
        C_D_Collider col{};
        PrefabLoader::ReadCollider(data, col);
        if (ovr.halfExtents) {
            col.half_x = ovr.halfExtents->x;
            col.half_y = ovr.halfExtents->y;
            col.half_z = ovr.halfExtents->z;
        }
        if (ovr.triVerts && ovr.triIndices) {
            col.type       = ColliderType::TriMesh;
            col.triVerts   = *ovr.triVerts;
            col.triIndices = *ovr.triIndices;
        }
        reg.Emplace<C_D_Collider>(id, std::move(col));
    });

    // ============================================================
    // C_D_Camera
    // ============================================================
    Register("C_D_Camera", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        C_D_Camera cam{};
        PrefabLoader::ReadCamera(data, cam);
        reg.Emplace<C_D_Camera>(id, cam);
    });

    // ============================================================
    // C_D_DebugName
    // ============================================================
    Register("C_D_DebugName", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides& ovr) {
        auto& dn = reg.Emplace<C_D_DebugName>(id);
        std::string name;
        if (data.contains("name") && data["name"].is_string())
            name = data["name"].get<std::string>();

        // 替换 _XX 为格式化的 spawnIndex
        if (ovr.spawnIndex) {
            auto pos = name.find("_XX");
            if (pos != std::string::npos) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "_%02d", *ovr.spawnIndex);
                name.replace(pos, 3, buf);
            }
        }

        strncpy_s(dn.name, sizeof(C_D_DebugName::name), name.c_str(), sizeof(C_D_DebugName::name) - 1);
    });

    // ============================================================
    // C_D_Health
    // ============================================================
    Register("C_D_Health", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        C_D_Health health{};
        if (data.contains("hp")    && data["hp"].is_number())    health.hp    = data["hp"].get<float>();
        if (data.contains("maxHp") && data["maxHp"].is_number()) health.maxHp = data["maxHp"].get<float>();
        reg.Emplace<C_D_Health>(id, health);
    });

    // ============================================================
    // C_D_AIPerception
    // ============================================================
    Register("C_D_AIPerception", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        auto& detect = reg.Emplace<C_D_AIPerception>(id);
        detect.detection_value = 0.0f;
        if (data.contains("detection_value_increase") && data["detection_value_increase"].is_number())
            detect.detection_value_increase = data["detection_value_increase"].get<float>();
        if (data.contains("detection_value_decrease") && data["detection_value_decrease"].is_number())
            detect.detection_value_decrease = data["detection_value_decrease"].get<float>();
    });

    // ============================================================
    // C_D_NavAgent
    // ============================================================
    Register("C_D_NavAgent", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_D_NavAgent>(id);
    });

    // ============================================================
    // C_D_HoloBaitState
    // ============================================================
    Register("C_D_HoloBaitState", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides& ovr) {
        auto& bait = reg.Emplace<C_D_HoloBaitState>(id);
        if (ovr.position)  bait.worldPos = *ovr.position;
        if (ovr.targetPos) bait.worldPos = *ovr.targetPos;
        if (data.contains("remainingTime") && data["remainingTime"].is_number())
            bait.remainingTime = data["remainingTime"].get<float>();
        if (data.contains("active") && data["active"].is_boolean())
            bait.active = data["active"].get<bool>();
    });

    // ============================================================
    // C_D_RoamAI
    // ============================================================
    Register("C_D_RoamAI", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides& ovr) {
        auto& roam = reg.Emplace<C_D_RoamAI>(id);
        if (ovr.targetPos) roam.targetPos = *ovr.targetPos;
        if (data.contains("roamSpeed")        && data["roamSpeed"].is_number())        roam.roamSpeed        = data["roamSpeed"].get<float>();
        if (data.contains("waypointInterval") && data["waypointInterval"].is_number()) roam.waypointInterval = data["waypointInterval"].get<float>();
        if (data.contains("detectRadius")     && data["detectRadius"].is_number())     roam.detectRadius     = data["detectRadius"].get<float>();
        if (data.contains("active")           && data["active"].is_boolean())          roam.active           = data["active"].get<bool>();
    });

    // ============================================================
    // 默认初始化组件（无参 Emplace）
    // ============================================================
    Register("C_D_PlayerState", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_D_PlayerState>(id, C_D_PlayerState{});
    });

    Register("C_D_Input", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_D_Input>(id, C_D_Input{});
    });

    Register("C_D_CQCState", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_D_CQCState>(id, C_D_CQCState{});
    });

    Register("C_D_AIState", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_D_AIState>(id);
    });

    Register("C_D_EnemyDormant", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_D_EnemyDormant>(id, C_D_EnemyDormant{});
    });

    // ============================================================
    // 纯标签组件
    // ============================================================
    Register("C_T_Player", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_Player>(id);
    });

    Register("C_T_Enemy", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_Enemy>(id);
    });

    Register("C_T_MainCamera", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_MainCamera>(id);
    });

    Register("C_T_InvisibleWall", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_InvisibleWall>(id);
    });

    Register("C_T_DeathZone", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_DeathZone>(id);
    });

    Register("C_T_TriggerZone", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_TriggerZone>(id);
    });

    Register("C_T_FinishZone", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_FinishZone>(id);
    });

    Register("C_T_Pathfinder", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_Pathfinder>(id);
    });

    Register("C_T_RoamAI", [](Registry& reg, EntityID id, const json&, const RuntimeOverrides&) {
        reg.Emplace<C_T_RoamAI>(id);
    });

    // ============================================================
    // 带数据标签组件
    // ============================================================
    Register("C_T_NavTarget", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        auto& target = reg.Emplace<C_T_NavTarget>(id);
        if (data.contains("target_type") && data["target_type"].is_string()) {
            std::string tt = data["target_type"].get<std::string>();
            strncpy_s(target.target_type, sizeof(target.target_type), tt.c_str(), sizeof(target.target_type) - 1);
        }
    });

    Register("C_T_ItemPickup", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        auto& pickup = reg.Emplace<C_T_ItemPickup>(id);
        if (data.contains("itemId") && data["itemId"].is_number())
            pickup.itemId = static_cast<ItemID>(data["itemId"].get<int>());
        if (data.contains("quantity") && data["quantity"].is_number())
            pickup.quantity = data["quantity"].get<uint8_t>();
    });

    // ============================================================
    // C_D_DataOceanPillar
    // ============================================================
    Register("C_D_DataOceanPillar", [](Registry& reg, EntityID id, const json& data, const RuntimeOverrides&) {
        C_D_DataOceanPillar pillar{};
        if (data.contains("baseY")      && data["baseY"].is_number())      pillar.baseY      = data["baseY"].get<float>();
        if (data.contains("amplitude")  && data["amplitude"].is_number())  pillar.amplitude  = data["amplitude"].get<float>();
        if (data.contains("phaseShift") && data["phaseShift"].is_number()) pillar.phaseShift = data["phaseShift"].get<float>();
        if (data.contains("sizeXZ")    && data["sizeXZ"].is_number())     pillar.sizeXZ     = data["sizeXZ"].get<float>();
        reg.Emplace<C_D_DataOceanPillar>(id, pillar);
    });

    LOG_INFO("[ComponentRegistry] RegisterAll: " << GetMap().size() << " components registered.");
}

} // namespace ECS
