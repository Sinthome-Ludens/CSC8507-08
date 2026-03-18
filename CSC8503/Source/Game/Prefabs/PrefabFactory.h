/**
 * @file PrefabFactory.h
 * @brief 实体预制体工厂声明。
 *
 * @details
 * 提供统一的实体构造入口，集中约束各类 Prefab 的组件组合与默认参数。
 */
#pragma once

#include "Core/ECS/Registry.h"
#include "Core/Bridge/AssetManager.h"
#include "Game/Components/C_D_Item.h"
#include "RuntimeOverrides.h"
#include "Vector.h"
#include "Quaternion.h"
#include <vector>
#include <string>

/**
 * @brief 实体预制体工厂（JSON 数据驱动 + 硬编码回退）
 *
 * 提供所有游戏实体的统一创建入口。
 *
 * 规范约束（游戏开发.md §实体约定）：
 *  严禁在 System 业务逻辑中直接调用 registry.Create()。
 *  所有实体创建必须通过此工厂的静态方法进行。
 *
 * 各 Create* 方法通过 PrefabLoader 从 Assets/Prefabs/ 的 JSON 蓝图读取默认参数，
 * 运行时参数（spawnPos, meshHandle 等）从函数参数覆盖。
 * 若 JSON 文件缺失或解析失败，自动回退到结构体内置默认值（与原硬编码一致）。
 *
 * @see Assets/Prefabs/Prefab_Camera_Main.json
 * @see Assets/Prefabs/Prefab_Env_Floor.json
 * @see Assets/Prefabs/Prefab_Player.json
 * @see Assets/Prefabs/Prefab_Physics_Cube.json
 * @see Assets/Prefabs/Prefab_Physics_Capsule.json
 * @see Assets/Prefabs/Prefab_Physics_Enemy.json
 * @see Assets/Prefabs/Prefab_Nav_Enemy.json
 * @see Assets/Prefabs/Prefab_Nav_Target.json
 * @see Assets/Prefabs/Prefab_Env_InvisibleWall.json
 * @see Assets/Prefabs/Prefab_Env_DeathZone.json
 * @see Assets/Prefabs/Prefab_TriggerZone.json
 * @see Assets/Prefabs/Prefab_Env_FinishZone.json
 * @see Assets/Prefabs/Prefab_HoloBait.json
 * @see Assets/Prefabs/Prefab_RoamAI.json
 * @see Assets/Prefabs/Prefab_Map_*.json
 */
class PrefabFactory {
public:
    PrefabFactory()  = delete; // 纯静态工厂，禁止实例化
    ~PrefabFactory() = delete;

    // ============================================================
    // 通用数据驱动创建入口
    // ============================================================

    /**
     * @brief 通用 Prefab 创建入口（数据驱动）
     *
     * 从 JSON 蓝图的 "Components" 字典中读取组件列表，
     * 通过 ComponentRegistry 查表 Emplace 各组件。
     *
     * @param reg             ECS Registry
     * @param prefabJsonFile  JSON 蓝图文件名（如 "Prefab_Player.json"）
     * @param overrides       运行时参数覆盖（mesh 句柄、位置等）
     * @return 创建的实体 ID，或 Entity::NULL_ENTITY（失败时）
     */
    static ECS::EntityID Create(
        ECS::Registry&          reg,
        const std::string&      prefabJsonFile,
        const ECS::RuntimeOverrides& overrides = {}
    );

    /**
     * @brief 从 JSON 蓝图的 "Variants" 中创建指定变体实体
     *
     * @param reg             ECS Registry
     * @param prefabJsonFile  JSON 蓝图文件名
     * @param variantName     变体名称（如 "Mesh", "Render", "Detect"）
     * @param overrides       运行时参数覆盖
     * @return 创建的实体 ID，或 Entity::NULL_ENTITY（失败时）
     */
    static ECS::EntityID CreateVariant(
        ECS::Registry&          reg,
        const std::string&      prefabJsonFile,
        const std::string&      variantName,
        const ECS::RuntimeOverrides& overrides = {}
    );

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

    /**
     * @brief 创建静态地图实体（PREFAB_ENV_STATIC_MAP）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_Material, C_D_RigidBody(Static),
     *       C_D_Collider(TriMesh), C_D_DebugName
     *
     * @param reg          ECS Registry
     * @param renderMesh   渲染用网格句柄（*.obj）
     * @param collVerts    碰撞 TriMesh 顶点（已缩放 + winding 修正后）
     * @param collIndices  碰撞 TriMesh 索引
     * @param worldOffset  世界空间偏移
     * @param scale        地图缩放系数
     * @return 地图实体 ID
     */
    static ECS::EntityID CreateStaticMapEntity(
        ECS::Registry&                          reg,
        ECS::MeshHandle                         renderMesh,
        const std::vector<NCL::Maths::Vector3>& collVerts,
        const std::vector<int>&                 collIndices,
        NCL::Maths::Vector3                     worldOffset,
        float                                   scale = 1.0f
    );

    /**
     * @brief @deprecated 创建纯渲染地图实体（无 Box 碰撞体，碰撞由 NavMeshFloor 提供）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_Material, C_D_DebugName
     * 不挂载 C_D_RigidBody 和 C_D_Collider — 碰撞完全由 NavMeshFloor TriMesh 承担。
     * 适用于碰撞箱与渲染 mesh 完全分离的场景。
     *
     * @param reg     ECS Registry
     * @param mapMesh 地图网格句柄
     * @param scale   地图缩放系数
     * @return 地图实体 ID
     */
    static ECS::EntityID CreateStaticMapRenderOnly(
        ECS::Registry&  reg,
        ECS::MeshHandle mapMesh,
        float           scale = 1.0f
    );

    /**
     * @brief 创建可见终点区域实体（渲染 + TriMesh Trigger 碰撞）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_RigidBody(Static),
     *       C_D_Collider(TriMesh, is_trigger), C_T_TriggerZone, C_D_DebugName
     *
     * 碰撞体使用 TriMesh 三角网格，形状与渲染 mesh 完全一致或来自独立碰撞数据。
     * 适用于终点区域等需要精确碰撞触发的场景。
     *
     * @param reg       ECS Registry
     * @param renderMesh  渲染用网格句柄
     * @param collisionVerts  碰撞三角网格顶点
     * @param collisionIndices  碰撞三角网格索引
     * @param worldOffset  世界空间偏移
     * @param scale  缩放系数
     * @return 终点区域实体 ID
     */
    static ECS::EntityID CreateFinishZoneMesh(
        ECS::Registry&                          reg,
        ECS::MeshHandle                         renderMesh,
        const std::vector<NCL::Maths::Vector3>& collisionVerts,
        const std::vector<int>&                 collisionIndices,
        NCL::Maths::Vector3                     worldOffset,
        float                                   scale = 1.0f
    );

    /**
     * @brief 创建终点区域渲染实体（纯视觉，无碰撞）
     *
     * 挂载：C_D_Transform, C_D_MeshRenderer, C_D_Material(红色), C_D_DebugName
     *
     * @param reg         ECS Registry
     * @param finishMesh  终点区域网格句柄
     * @param worldOffset 世界空间偏移
     * @param scale       缩放系数
     * @return 渲染实体 ID
     */
    static ECS::EntityID CreateFinishZoneRender(
        ECS::Registry&      reg,
        ECS::MeshHandle     finishMesh,
        NCL::Maths::Vector3 worldOffset,
        float               scale = 1.0f
    );

    /**
     * @brief 创建终点区域检测实体（不可见，仅供 Sys_LevelGoal 距离检测）
     *
     * 挂载：C_D_Transform, C_T_FinishZone, C_D_DebugName
     *
     * @param reg       ECS Registry
     * @param detectPos 检测点世界坐标
     * @return 检测实体 ID
     */
    static ECS::EntityID CreateFinishZoneDetect(
        ECS::Registry&      reg,
        NCL::Maths::Vector3 detectPos
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

    /**
     * @brief 创建静态 Trigger 区域实体（PREFAB_TRIGGER_ZONE）。
     *
     * @details
     * 挂载：C_D_Transform, C_D_RigidBody, C_D_Collider, C_T_TriggerZone, C_D_DebugName。
     * 不挂载 MeshRenderer，仅用于触发检测，不参与可视化渲染。
     *
     * @param reg         ECS Registry
     * @param position    Trigger 中心世界坐标
     * @param halfExtents Box Trigger 半尺寸
     * @return Trigger 区域实体 ID
     */
    static ECS::EntityID CreateTriggerZone(
        ECS::Registry&      reg,
        NCL::Maths::Vector3 position,
        NCL::Maths::Vector3 halfExtents
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
    // 巡逻路线挂载
    // ============================================================

    /**
     * @brief 为已创建的敌人实体挂载巡逻路线并设置初始朝向
     *
     * 挂载：C_D_PatrolRoute
     * 修改：C_D_Transform::rotation（面向第二个巡逻路点）
     *
     * @param reg        ECS Registry
     * @param entity     目标敌人实体
     * @param waypoints  世界空间巡逻路点数组
     * @param count      有效路点数量
     * @param spawnPos   敌人生成位置（用于计算初始朝向）
     */
    static void AttachPatrolRoute(
        ECS::Registry&             reg,
        ECS::EntityID              entity,
        const NCL::Maths::Vector3* waypoints,
        int                        count,
        NCL::Maths::Vector3        spawnPos
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
     * @brief @deprecated 从 NavMesh 可行走三角形创建静态地板碰撞体（PREFAB_ENV_NAVMESH_FLOOR）
     *
     * 挂载：C_D_Transform, C_D_RigidBody(Static), C_D_Collider(TriMesh), C_D_DebugName
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

    // ============================================================
    // 道具效果实体
    // ============================================================

    /**
     * @brief 创建全息诱饵实体（PREFAB_HOLO_BAIT）
     *
     * 挂载：C_D_Transform, C_D_HoloBaitState, C_D_DebugName
     *
     * @param reg       ECS Registry
     * @param worldPos  诱饵世界位置
     * @return 诱饵实体 ID
     */
    static ECS::EntityID CreateHoloBait(
        ECS::Registry&      reg,
        NCL::Maths::Vector3 worldPos
    );

    /**
     * @brief 创建流窜 AI 实体（PREFAB_ROAM_AI）
     *
     * 挂载：C_D_Transform, C_T_RoamAI, C_D_RoamAI, C_D_DebugName
     *
     * @param reg        ECS Registry
     * @param targetPos  目标世界位置
     * @return 流窜 AI 实体 ID
     */
    static ECS::EntityID CreateRoamAI(
        ECS::Registry&      reg,
        NCL::Maths::Vector3 targetPos
    );
};
