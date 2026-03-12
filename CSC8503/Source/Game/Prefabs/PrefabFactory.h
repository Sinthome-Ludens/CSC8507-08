#pragma once

#include "Core/ECS/Registry.h"
#include "Core/Bridge/AssetManager.h"
#include "Vector.h"
#include "Quaternion.h"
#include <vector>

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

    /**
     * @brief 创建静态地图渲染实体（通用，适用于所有场景地图）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody, C_D_Collider,
     *       C_D_Material, C_D_DebugName
     *
     * 仅添加渲染组件，不创建精确物理碰撞体。
     * 物理支撑由 CreateNavMeshFloor 的三角网格地板提供。
     *
     * @param reg     ECS Registry
     * @param mapMesh 地图网格句柄（由 AssetManager::LoadMesh 获取）
     * @param scale   地图缩放系数（TutorialLevel=2.0，其余场景=1.0）
     * @return 地图实体 ID
     */
    static ECS::EntityID CreateStaticMap(
        ECS::Registry&  reg,
        ECS::MeshHandle mapMesh,
        float           scale = 1.0f
    );

    // ============================================================
    // 玩家
    // ============================================================

    /**
     * @brief 创建玩家实体（PREFAB_PLAYER）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody, C_D_Collider,
     *       C_T_Player, C_D_PlayerState, C_D_Input, C_D_DebugName
     *
     * @param reg       ECS Registry
     * @param cubeMesh  临时网格句柄（后续替换为角色模型）
     * @param spawnPos  生成位置（世界坐标）
     * @return 玩家实体 ID
     */
    static ECS::EntityID CreatePlayer(
        ECS::Registry&      reg,
        ECS::MeshHandle     cubeMesh,
        NCL::Maths::Vector3 spawnPos
    );

    // ============================================================
    // 隐形碰撞墙
    // ============================================================

    /**
     * @brief 创建隐形碰撞墙实体（PREFAB_ENV_INVISIBLE_WALL）
     *
     * 挂载：C_D_Transform, C_D_RigidBody, C_D_Collider, C_T_InvisibleWall, C_D_DebugName
     * 不挂载 C_D_MeshRenderer → 渲染不可见，但 Sys_Physics 仍创建 Jolt 碰撞体。
     *
     * @param reg         ECS Registry
     * @param wallIndex   墙壁序号（用于 DebugName 编号）
     * @param position    墙壁中心世界坐标
     * @param halfExtents Box 碰撞体半尺寸
     * @param rotation    初始旋转（默认无旋转）
     * @return 隐形墙实体 ID
     */
    static ECS::EntityID CreateInvisibleWall(
        ECS::Registry&         reg,
        int                    wallIndex,
        NCL::Maths::Vector3    position,
        NCL::Maths::Vector3    halfExtents,
        NCL::Maths::Quaternion rotation = NCL::Maths::Quaternion(0.0f, 0.0f, 0.0f, 1.0f)
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

    // ============================================================
    // 导航实体（NavTest 场景专用，来自 feat/navtest-scene）
    // ============================================================

    /**
     * @brief 创建带 NavAgent 的导航敌人实体（PREFAB_NAV_ENEMY）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody(锁旋转), C_D_Collider(Capsule),
     *        C_T_Enemy, C_D_AIState, C_D_AIPerception,
     *        C_D_NavAgent, C_T_Pathfinder, C_D_DebugName
     *
     * @param reg          ECS Registry
     * @param enemyMesh    敌人网格句柄
     * @param spawnIndex   生成序号
     * @param spawnPos     生成位置（世界坐标）
     * @return 导航敌人实体 ID
     */
    static ECS::EntityID CreateNavEnemy(
        ECS::Registry&      reg,
        ECS::MeshHandle     enemyMesh,
        int                 spawnIndex,
        NCL::Maths::Vector3 spawnPos
    );

    /**
     * @brief 创建导航目标实体（PREFAB_NAV_TARGET）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody(静态), C_D_Collider(Box),
     *        C_T_NavTarget, C_D_DebugName
     *
     * @param reg          ECS Registry
     * @param targetMesh   目标网格句柄
     * @param spawnIndex   生成序号
     * @param spawnPos     生成位置（世界坐标）
     * @return 目标实体 ID
     */
    static ECS::EntityID CreateNavTarget(
        ECS::Registry&      reg,
        ECS::MeshHandle     targetMesh,
        int                 spawnIndex,
        NCL::Maths::Vector3 spawnPos
    );

    // ============================================================
    // 死亡区域
    // ============================================================

    /**
     * @brief 创建即死触发区域实体（PREFAB_ENV_DEATH_ZONE）
     *
     * 挂载：C_D_Transform, C_D_RigidBody(Static), C_D_Collider(Box, is_trigger=true),
     *       C_T_DeathZone, C_D_DebugName
     * 不挂载 C_D_MeshRenderer → 渲染不可见。
     *
     * @param reg         ECS Registry
     * @param zoneIndex   区域序号（用于 DebugName 编号）
     * @param position    触发器中心世界坐标
     * @param halfExtents Box 触发器半尺寸
     * @return 死亡区域实体 ID
     */
    static ECS::EntityID CreateDeathZone(
        ECS::Registry&      reg,
        int                 zoneIndex,
        NCL::Maths::Vector3 position,
        NCL::Maths::Vector3 halfExtents
    );

    /**
     * @brief 创建动态物理胶囊体实体（PREFAB_PHYSICS_CAPSULE，来自 master）
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

    // ============================================================
    // NavMesh 地板碰撞体
    // ============================================================

    /**
     * @brief 从 NavMesh 可行走三角形创建静态地板碰撞体（PREFAB_ENV_NAVMESH_FLOOR）
     *
     * 挂载：C_D_Transform, C_D_RigidBody(Static), C_D_TriMeshCollider, C_D_DebugName
     * 不挂载 C_D_Collider（使用 TriMesh 路径）。
     *
     * 适用于多层地图（HangerA/HangerB/Lab 等），为斜坡和上层平台提供精确物理支撑。
     * 顶点坐标应为 NavMesh 局部空间（已 ScaleVertices），worldOffset 提供 Y 偏移对齐。
     *
     * @param reg         ECS Registry
     * @param vertices    NavMesh 可行走顶点列表（局部空间）
     * @param indices     三角形索引列表（每 3 个为一个三角形）
     * @param worldOffset 体的世界偏移（通常为 (0, -6*kMapScale, 0)）
     * @return 地板实体 ID
     */
    static ECS::EntityID CreateNavMeshFloor(
        ECS::Registry&                          reg,
        const std::vector<NCL::Maths::Vector3>& vertices,
        const std::vector<int>&                 indices,
        NCL::Maths::Vector3                     worldOffset
    );
};