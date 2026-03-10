#pragma once
#include "Game/Utils/PathfinderUtil.h"
#include <vector>
#include <string>

namespace ECS {

/**
 * @brief NavMesh 三角形数据（内部使用）
 */
struct NavTriangle {
    int  v[3];           ///< 顶点索引（m_Vertices 中）
    int  neighbors[3];   ///< 相邻三角形索引（-1 = 无）
    NCL::Maths::Vector3 centroid; ///< 三角形重心（预计算）
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

    /// 计算每个三角形的重心并建立邻接表
    void BuildAdjacency();

    /// 返回重心距 p 最近的三角形索引
    int  FindNearestTriangle(const NCL::Maths::Vector3& p) const;

    /// 在三角形图上执行 A* 搜索，outTriPath 为三角形索引序列
    bool AStarSearch(int startTri, int endTri,
                     std::vector<int>& outTriPath) const;
};

} // namespace ECS
