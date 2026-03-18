/**
 * @file PrefabLoader.h
 * @brief JSON Prefab 加载工具：从 Assets/Prefabs/ 读取 JSON 文件，填充各 Prefab 默认值结构体。
 *
 * @details
 * 每种 Prefab 对应一个 Defaults 结构体，其成员默认值与 PrefabFactory.cpp 中的硬编码值完全一致。
 * Load*Defaults 函数尝试从 JSON 文件中读取字段覆盖默认值；若文件不存在或字段缺失，
 * 结构体保留其默认值，保证行为与当前硬编码路径完全相同。
 *
 * 命名空间：ECS::PrefabLoader
 * 依赖：nlohmann/json, C_D_RigidBody, C_D_Collider, C_D_Camera, MapLoadConfig
 */
#pragma once

#include "Game/Components/C_D_RigidBody.h"
#include "Game/Components/C_D_Collider.h"
#include "Game/Components/C_D_Camera.h"
#include "Game/Components/MapLoadConfig.h"
#include "Vector.h"
#include "Quaternion.h"

#include <nlohmann/json_fwd.hpp>
#include <string>

namespace ECS {
namespace PrefabLoader {

/**
 * @brief 返回 Prefab JSON 文件所在目录的完整路径（含末尾斜杠）。
 */
const std::string& PrefabDir();

/**
 * @brief 清除内部 JSON 文件缓存（场景切换时调用，释放内存）。
 */
void ClearCache();

/* ================================================================
 * 公开 Read 辅助函数（供 ComponentRegistry 使用）
 * ================================================================ */

/**
 * @brief 从 JSON 对象中读取 [x,y,z] 数组到 Vector3。
 * @param j    JSON 对象
 * @param key  字段名
 * @param out  目标向量；若 key 不存在或格式不符，保持原值不变
 */
void ReadVec3(const nlohmann::json& j, const char* key, NCL::Maths::Vector3& out);

/**
 * @brief 从 JSON 对象中读取 [x,y,z,w] 数组到 Vector4。
 * @param j    JSON 对象
 * @param key  字段名
 * @param out  目标向量；若 key 不存在或格式不符，保持原值不变
 */
void ReadVec4(const nlohmann::json& j, const char* key, NCL::Maths::Vector4& out);

/**
 * @brief 从 JSON 对象中读取 [x,y,z,w] 数组到 Quaternion。
 * @param j    JSON 对象
 * @param key  字段名
 * @param out  目标四元数；若 key 不存在或格式不符，保持原值不变
 */
void ReadQuat(const nlohmann::json& j, const char* key, NCL::Maths::Quaternion& out);

/**
 * @brief 从 JSON 对象中读取 RigidBody 各字段到 C_D_RigidBody。
 * @param j   JSON 对象（应为 "C_D_RigidBody" 子对象）
 * @param rb  目标刚体组件；缺失字段保持原值不变
 */
void ReadRigidBody(const nlohmann::json& j, C_D_RigidBody& rb);

/**
 * @brief 从 JSON 对象中读取 Collider 各字段到 C_D_Collider。
 *
 * Capsule 类型使用 "radius"/"half_height" 字段名，
 * Box/Sphere 使用 "half_x"/"half_y"/"half_z"。
 *
 * @param j    JSON 对象（应为 "C_D_Collider" 子对象）
 * @param col  目标碰撞组件；缺失字段保持原值不变
 */
void ReadCollider(const nlohmann::json& j, C_D_Collider& col);

/**
 * @brief 从 JSON 对象中读取 Camera 各字段到 C_D_Camera。
 * @param j    JSON 对象（应为 "C_D_Camera" 子对象）
 * @param cam  目标相机组件；缺失字段保持原值不变
 */
void ReadCamera(const nlohmann::json& j, C_D_Camera& cam);

/**
 * @brief 加载 JSON 蓝图文件（缓存机制），返回 JSON 文档指针。
 *
 * 返回的指针指向内部缓存，生命周期持续到 ClearCache() 被调用。
 * 文件不存在或解析失败时返回 nullptr 并输出 LOG_WARN/LOG_ERROR。
 *
 * @param filename JSON 文件名（不含路径前缀，如 "Prefab_Player.json"）
 * @return 指向缓存中 JSON 文档的指针，或 nullptr
 */
const nlohmann::json* LoadBlueprint(const std::string& filename);

/* ================================================================
 * Prefab 默认值结构体
 * 所有默认值与 PrefabFactory.cpp 硬编码值严格对齐
 * ================================================================ */

struct PrefabCameraDefaults {
    NCL::Maths::Vector3 position{0.0f, 15.0f, 40.0f};
    float pitch = -20.0f;
    float yaw   = 0.0f;

    C_D_Camera camera{
        45.0f,
        1.0f,
        1000.0f,
        -20.0f,
        0.0f,
        20.0f,
        0.5f,
        false
    };
};

struct PrefabFloorDefaults {
    NCL::Maths::Vector3 position{0.0f, -6.0f, 0.0f};
    NCL::Maths::Vector3 scale{50.0f, 1.0f, 50.0f};

    C_D_RigidBody rb{
        1.0f,
        0.05f,
        0.05f,
        1.0f,
        true,
        false,
        false, false, false,
        false
    };

    C_D_Collider col{
        ColliderType::Box,
        50.0f, 1.0f, 50.0f,
        0.5f,
        0.0f,
        false,
        {}, {}
    };
};

struct PrefabPlayerDefaults {
    C_D_RigidBody rb{
        5.0f,
        0.5f,
        0.05f,
        1.0f,
        false, false,
        true, true, true,
        false
    };

    C_D_Collider col{
        ColliderType::Capsule,
        0.5f, 1.0f, 0.5f,
        0.5f,
        0.0f,
        false,
        {}, {}
    };

    float hp    = 100.0f;
    float maxHp = 100.0f;
};

struct PrefabPhysicsCubeDefaults {
    C_D_RigidBody rb{
        1.0f,
        0.05f,
        0.05f,
        1.0f,
        false, false,
        false, false, false,
        false
    };

    C_D_Collider col{
        ColliderType::Box,
        1.0f, 1.0f, 1.0f,
        0.5f,
        0.1f,
        false,
        {}, {}
    };
};

struct PrefabPhysicsCapsuleDefaults {
    NCL::Maths::Vector3 scale{0.4225f, 0.4704f, 0.4225f};

    C_D_RigidBody rb{
        1.0f,
        0.05f,
        0.05f,
        1.0f,
        false, false,
        true, false, true,
        false
    };

    C_D_Collider col{
        ColliderType::Capsule,
        0.5f, 0.5f, 0.5f,
        0.5f,
        0.0f,
        false,
        {}, {}
    };
};

struct PrefabEnemyDefaults {
    C_D_RigidBody rb{
        1.0f,
        0.05f,
        0.05f,
        1.0f,
        false, false,
        true, false, true,
        false
    };

    C_D_Collider col{};

    float detection_increase = 15.0f;
    float detection_decrease = 5.0f;
};

struct PrefabNavTargetDefaults {
    NCL::Maths::Vector3 scale{0.5f, 0.5f, 0.5f};

    C_D_Collider col{
        ColliderType::Box,
        0.5f, 0.5f, 0.5f,
        0.5f,
        0.0f,
        false,
        {}, {}
    };
};

struct PrefabInvisibleWallDefaults {
    C_D_Collider col{
        ColliderType::Box,
        0.5f, 0.5f, 0.5f,
        0.0f,
        0.0f,
        false,
        {}, {}
    };
};

struct PrefabDeathZoneDefaults {
    C_D_Collider col{
        ColliderType::Box,
        0.5f, 0.5f, 0.5f,
        0.0f,
        0.0f,
        true,
        {}, {}
    };
};

struct PrefabTriggerZoneDefaults {
    C_D_Collider col{
        ColliderType::Box,
        0.5f, 0.5f, 0.5f,
        0.5f,
        0.0f,
        true,
        {}, {}
    };
};

struct PrefabHoloBaitDefaults {
    NCL::Maths::Vector3 scale{0.5f, 0.5f, 0.5f};
    float remainingTime = 3.0f;
};

struct PrefabRoamAIDefaults {
    NCL::Maths::Vector3 scale{0.4f, 0.4f, 0.4f};
    float roamSpeed         = 6.0f;
    float waypointInterval  = 2.0f;
    float detectRadius      = 1.5f;
};

/* ================================================================
 * Load 函数声明
 * 返回 true 表示 JSON 文件读取成功（字段可能部分覆盖）；
 * 返回 false 表示文件不存在或解析失败，out 保留默认值。
 * ================================================================ */

bool LoadMapConfig(const std::string& prefabName, MapLoadConfig& out);

bool LoadCameraDefaults(PrefabCameraDefaults& out);
bool LoadFloorDefaults(PrefabFloorDefaults& out);
bool LoadPlayerDefaults(PrefabPlayerDefaults& out);
bool LoadPhysicsCubeDefaults(PrefabPhysicsCubeDefaults& out);
bool LoadPhysicsCapsuleDefaults(PrefabPhysicsCapsuleDefaults& out);
bool LoadPhysicsEnemyDefaults(PrefabEnemyDefaults& out);
bool LoadNavEnemyDefaults(PrefabEnemyDefaults& out);
bool LoadNavTargetDefaults(PrefabNavTargetDefaults& out);
bool LoadInvisibleWallDefaults(PrefabInvisibleWallDefaults& out);
bool LoadDeathZoneDefaults(PrefabDeathZoneDefaults& out);
bool LoadTriggerZoneDefaults(PrefabTriggerZoneDefaults& out);
bool LoadHoloBaitDefaults(PrefabHoloBaitDefaults& out);
bool LoadRoamAIDefaults(PrefabRoamAIDefaults& out);

} // namespace PrefabLoader
} // namespace ECS
