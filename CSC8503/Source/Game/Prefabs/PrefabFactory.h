#pragma once

#include "Core/ECS/Registry.h"
#include "Core/Bridge/AssetManager.h"
#include "Vector.h"
#include "Quaternion.h"

/**
 * @brief 实体预制体工厂（硬编码模式）
 *
 * 提供所有游戏实体的统一创建入口。
 *
 * 规范约束（游戏开发.md §实体约定）：
 *  严禁在 System 业务逻辑中直接调用 registry.Create()。
 *  所有实体创建必须通过此工厂的静态方法进行。
 *
 * 当前为"硬编码工厂"模式；对应 JSON 蓝图位于 Assets/Prefabs/，
 * 待反射系统完成后将迁移为 JSON 数据驱动模式。
 *
 * @see Assets/Prefabs/Prefab_Camera_Main.json
 * @see Assets/Prefabs/Prefab_Env_Floor.json
 * @see Assets/Prefabs/Prefab_Physics_Cube.json
 * @see Assets/Prefabs/Prefab_Physics_Capsule.json
 */
class PrefabFactory {
public:
    PrefabFactory()  = delete; // 纯静态工厂，禁止实例化
    ~PrefabFactory() = delete;

    // ============================================================
    // 相机
    // ============================================================

    /**
     * @brief 创建主相机实体（PREFAB_CAMERA_MAIN）
     *
     * 挂载：C_D_Transform, C_D_Camera, C_T_MainCamera, C_D_DebugName
     *
     * @param reg       ECS Registry
     * @param position 初始世界坐标
     * @param pitch    初始俯仰角（度，向下为负）
     * @param yaw      初始偏航角（度）
     * @return 相机实体 ID
     */
    static ECS::EntityID CreateCameraMain(
        ECS::Registry&      reg,
        NCL::Maths::Vector3 position = NCL::Maths::Vector3(0.0f, 15.0f, 40.0f),
        float               pitch    = -20.0f,
        float               yaw      = 0.0f
    );

    // ============================================================
    // 环境
    // ============================================================

    /**
     * @brief 创建静态地板实体（PREFAB_ENV_FLOOR）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody, C_D_Collider, C_D_DebugName
     *
     * @param reg       ECS Registry
     * @param cubeMesh  立方体网格句柄
     * @return 地板实体 ID
     */
    static ECS::EntityID CreateFloor(
        ECS::Registry&  reg,
        ECS::MeshHandle cubeMesh
    );

    // ============================================================
    // 动态物体
    // ============================================================

    /**
     * @brief 创建动态物理方块实体（PREFAB_PHYSICS_CUBE）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody, C_D_Collider, C_D_DebugName
     *
     * @param reg        ECS Registry
     * @param cubeMesh   立方体网格句柄
     * @param spawnIndex 生成序号（用于 DebugName 编号）
     * @param spawnPos   生成位置（世界坐标）
     * @return 方块实体 ID
     */
    static ECS::EntityID CreatePhysicsCube(
        ECS::Registry&      reg,
        ECS::MeshHandle     cubeMesh,
        int                 spawnIndex,
        NCL::Maths::Vector3 spawnPos
    );

    /**
     * @brief 创建带 EnemyAI 组件的敌人实体（PREFAB_PHYSICS_ENEMY）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody(锁旋转), C_D_Collider(Capsule),
     *        C_T_Enemy, C_D_AIState, C_D_AIPerception, C_D_DebugName
     *
     * @param reg           ECS Registry
     * @param enemyMesh     敌人网格句柄
     * @param spawnIndex    生成序号
     * @param spawnPos      生成位置（世界坐标）
     * @return 敌人实体 ID
     */
    static ECS::EntityID CreatePhysicsEnemy(
        ECS::Registry&      reg,
        ECS::MeshHandle     enemyMesh,
        int                 spawnIndex,
        NCL::Maths::Vector3 spawnPos
    );

    /**
     * @brief 创建动态物理胶囊体实体（PREFAB_PHYSICS_CAPSULE）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody, C_D_Collider, C_D_DebugName
     *
     * Collider：ColliderType::Capsule，radius = 0.5，half_height = 0.5
     * 总高度 = 2 * half_height + 2 * radius = 2.0
     *
     * @param reg           ECS Registry
     * @param capsuleMesh   胶囊体网格句柄
     * @param spawnIndex    生成序号（用于 DebugName 编号）
     * @param spawnPos      生成位置（世界坐标）
     * @return 胶囊体实体 ID
     */
    static ECS::EntityID CreatePhysicsCapsule(
        ECS::Registry&      reg,
        ECS::MeshHandle     capsuleMesh,
        int                 spawnIndex,
        NCL::Maths::Vector3 spawnPos
    );
};