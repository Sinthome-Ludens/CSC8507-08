#pragma once
#include "Game/Utils/PathfinderUtil.h"
#include <vector>
#include <string>
#include <fstream>

namespace ECS {

/**
 * @brief NavMesh 边界边数据（邻居为 -1 的三角形边 = 墙面位置）
 * 用于在场景中自动生成隐形墙体碰撞体。
 */
struct BoundaryEdge {
    NCL::Maths::Vector3 v0, v1;    ///< 两端点（navmesh 局部坐标系）
    NCL::Maths::Vector3 midpoint;  ///< 边中点
    float length;                  ///< 边长（XZ 平面）
    float dirX, dirZ;              ///< 归一化边方向（XZ，用于计算旋转）
};

/**
 * @brief NavMesh 区域类型
 * 0 = 可行走，1 = 不可通行，2+ = 自定义（高代价等）
 */
enum class NavArea : int {
    Walkable    = 0,
    NotWalkable = 1,
};

/**
 * @brief NavMesh 三角形数据（内部使用）
 */
struct NavTriangle {
    int  v[3];           ///< 顶点索引（m_Vertices 中）
    int  neighbors[3];   ///< 相邻三角形索引（-1 = 无）
    NCL::Maths::Vector3 centroid; ///< 三角形重心（预计算）
    int  area = 0;       ///< 区域类型（0=可行走，1=不可通行）
    float slope_angle = 0.0f; ///< 坡度角（0=平地，90=垂直墙，用于 A* 代价调整）
};

/**
 * @brief NavMesh A* 寻路实现
 *
 * 从 .navmesh 文件加载三角网格，使用 A* 在三角形图上搜索路径。
 * 若未加载 navmesh，则退化为直线追踪（兼容旧行为）。
 *
 * 注意：此工具类不是 ECS 组件，使用 std::vector 是合法的。
 *
 * 日后可升级为 funnel algorithm 以平滑路径（接口不变）。
 */
class NavMeshPathfinderUtil : public PathfinderUtil {
public:
    NavMeshPathfinderUtil() = default;

    /**
     * @brief 从 .navmesh 文件加载三角网格
     * @param filePath 文件绝对路径
     * @return 加载成功返回 true；失败时退化为直线追踪
     */
    bool LoadNavMesh(const std::string& filePath);

    /// 是否已成功加载 navmesh
    bool IsLoaded() const { return m_Loaded; }

    /**
     * @brief 对所有顶点坐标等比例缩放（必须在 LoadNavMesh 之后、GetBoundaryEdges 之前调用）
     *
     * 用于使 navmesh 寻路坐标与经过 scale 变换的地图物理世界对齐。
     * 同步更新三角形重心。
     *
     * @param scale 缩放倍率（与 CreateStaticMap 的 scale 保持一致）
     */
    void ScaleVertices(float scale);

    /**
     * @brief 提取 navmesh 的所有边界边（无邻居的三角形边 = 墙壁位置）
     *
     * 用于在场景初始化时自动创建隐形墙体碰撞体。
     * 每条边界边对应一面墙壁，包含中点、长度和方向信息。
     *
     * @return 边界边列表（navmesh 局部坐标系下）
     */
    std::vector<BoundaryEdge> GetBoundaryEdges() const;

    /**
     * @brief 提取所有可行走三角形的顶点和索引（area == Walkable）
     *
     * 用于生成 NavMesh 地板物理碰撞体（配合 PrefabFactory::CreateNavMeshFloor）。
     * 坐标为 navmesh 局部空间，已经过 ScaleVertices 处理。
     * 必须在 LoadNavMesh 和 ScaleVertices 之后调用。
     *
     * @param outVerts   输出顶点列表（局部空间）
     * @param outIndices 输出索引列表（每 3 个为一个三角形）
     */
    void GetWalkableGeometry(std::vector<NCL::Maths::Vector3>& outVerts,
                              std::vector<int>& outIndices) const;

    /**
     * @brief PathfinderUtil 接口：A* 寻路
     *
     * 若 navmesh 未加载，退化为直线追踪（输出单路点 {end}）。
     * 输出路径长度不超过调用方（CopyPathToAgent）的 NAV_MAX_WAYPOINTS 上限。
     */
    bool FindPath(const NCL::Maths::Vector3& start,
                  const NCL::Maths::Vector3& end,
                  std::vector<NCL::Maths::Vector3>& outPath) override;

private:
    std::vector<NCL::Maths::Vector3> m_Vertices;
    std::vector<NavTriangle>          m_Triangles;
    bool                              m_Loaded = false;

    /// 解析命名节格式（vertexCount / indexCount / vertices / indices / areas）
    bool LoadNamedFormat(std::ifstream& file);

    /// 解析纯数字格式（含内嵌邻居数据）
    bool LoadRawFormat(std::ifstream& file);

    /// 计算每个三角形的重心并建立邻接表（仅命名格式需要）
    void BuildAdjacency();

    /// 返回重心距 p 最近的可行走三角形索引
    int  FindNearestTriangle(const NCL::Maths::Vector3& p) const;

    /// 在三角形图上执行 A* 搜索，outTriPath 为三角形索引序列
    bool AStarSearch(int startTri, int endTri,
                     std::vector<int>& outTriPath) const;
};

} // namespace ECS
