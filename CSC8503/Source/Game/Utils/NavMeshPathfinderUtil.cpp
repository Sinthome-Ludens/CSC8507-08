/**
 * @file NavMeshPathfinderUtil.cpp
 * @brief NavMesh A* 寻路实现（navmesh 文件解析 + 三角形图搜索）。
 */
#include "NavMeshPathfinderUtil.h"

#include "Game/Utils/Log.h"

#include <fstream>
#include <sstream>
#include <queue>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ECS {

// ============================================================
// LoadNavMesh — 解析 .navmesh 文本文件
// 格式：
//   vertexCount N  / indexCount M
//   vertices\n  x y z × N
//   indices\n   i1 i2 i3 × M （indexCount = 三角形数）
//   areas\n     area_id × M  （可选，忽略）
// ============================================================
bool NavMeshPathfinderUtil::LoadNavMesh(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_WARN("[NavMeshPathfinderUtil] Cannot open navmesh: " << filePath);
        return false;
    }

    m_Vertices.clear();
    m_Triangles.clear();
    m_Loaded = false;

    int vertexCount = 0;
    int indexCount  = 0;

    std::string line;

    // ── 1. 读取文件头（vertexCount / indexCount） ─────────────────────────
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "vertexCount") {
            ss >> vertexCount;
        } else if (token == "indexCount") {
            ss >> indexCount;
        } else if (token == "vertices") {
            break; // 进入顶点读取阶段
        }
    }

    if (vertexCount <= 0 || indexCount <= 0) {
        LOG_WARN("[NavMeshPathfinderUtil] Invalid header in: " << filePath);
        return false;
    }

    // ── 2. 读取顶点（x y z） ─────────────────────────────────────────────
    m_Vertices.reserve(vertexCount);
    int vRead = 0;
    while (vRead < vertexCount && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        float x, y, z;
        if (ss >> x >> y >> z) {
            m_Vertices.push_back(NCL::Maths::Vector3(x, y, z));
            ++vRead;
        }
    }

    // ── 3. 跳到 indices 节 ────────────────────────────────────────────────
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("indices") != std::string::npos) break;
    }

    // ── 4. 读取三角形（i1 i2 i3 per line，indexCount = 三角形数） ─────────
    m_Triangles.reserve(indexCount);
    int tRead = 0;
    while (tRead < indexCount && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        NavTriangle tri;
        if (ss >> tri.v[0] >> tri.v[1] >> tri.v[2]) {
            tri.neighbors[0] = tri.neighbors[1] = tri.neighbors[2] = -1;
            tri.centroid.x = tri.centroid.y = tri.centroid.z = 0.0f;
            m_Triangles.push_back(tri);
            ++tRead;
        }
    }

    if (vRead < vertexCount || tRead < indexCount) {
        LOG_WARN("[NavMeshPathfinderUtil] Incomplete data in: " << filePath
                 << " (verts=" << vRead << "/" << vertexCount
                 << " tris=" << tRead << "/" << indexCount << ")");
        return false;
    }

    // ── 5. 建立邻接表 + 计算重心 ──────────────────────────────────────────
    BuildAdjacency();

    m_Loaded = true;
    LOG_INFO("[NavMeshPathfinderUtil] Loaded navmesh: " << filePath
             << " (" << vertexCount << " verts, " << indexCount << " tris)");
    return true;
}

// ============================================================
// BuildAdjacency — 计算每个三角形的重心，并建立边邻接关系
// 两个三角形共享 2 个顶点索引则视为相邻。
// O(N²) 对 N ≤ 500 的 navmesh 足够快（< 1ms）。
// ============================================================
void NavMeshPathfinderUtil::BuildAdjacency()
{
    int N = static_cast<int>(m_Triangles.size());

    // 计算重心
    for (int i = 0; i < N; ++i) {
        NavTriangle& t = m_Triangles[i];
        const NCL::Maths::Vector3& a = m_Vertices[t.v[0]];
        const NCL::Maths::Vector3& b = m_Vertices[t.v[1]];
        const NCL::Maths::Vector3& c = m_Vertices[t.v[2]];
        t.centroid.x = (a.x + b.x + c.x) / 3.0f;
        t.centroid.y = (a.y + b.y + c.y) / 3.0f;
        t.centroid.z = (a.z + b.z + c.z) / 3.0f;
    }

    // 建立邻接：共享 2 个顶点 → 相邻
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            int shared = 0;
            for (int a = 0; a < 3; ++a)
                for (int b = 0; b < 3; ++b)
                    if (m_Triangles[i].v[a] == m_Triangles[j].v[b])
                        ++shared;

            if (shared < 2) continue;

            // 将 j 写入 i 的第一个空邻接槽
            for (int s = 0; s < 3; ++s) {
                if (m_Triangles[i].neighbors[s] == -1) {
                    m_Triangles[i].neighbors[s] = j;
                    break;
                }
            }
            // 将 i 写入 j 的第一个空邻接槽
            for (int s = 0; s < 3; ++s) {
                if (m_Triangles[j].neighbors[s] == -1) {
                    m_Triangles[j].neighbors[s] = i;
                    break;
                }
            }
        }
    }
}

// ============================================================
// FindNearestTriangle — 返回重心距 p 最近的三角形索引
// ============================================================
int NavMeshPathfinderUtil::FindNearestTriangle(const NCL::Maths::Vector3& p) const
{
    int   best  = 0;
    float bestD = 1e30f;

    for (int i = 0; i < static_cast<int>(m_Triangles.size()); ++i) {
        const NCL::Maths::Vector3& c = m_Triangles[i].centroid;
        float dx = p.x - c.x;
        float dy = p.y - c.y;
        float dz = p.z - c.z;
        float d  = dx*dx + dy*dy + dz*dz;
        if (d < bestD) {
            bestD = d;
            best  = i;
        }
    }

    return best;
}

// ============================================================
// AStarSearch — 在三角形图上搜索 startTri → endTri
// ============================================================
bool NavMeshPathfinderUtil::AStarSearch(int startTri, int endTri,
                                        std::vector<int>& outTriPath) const
{
    int N = static_cast<int>(m_Triangles.size());

    std::vector<float> gCost(N, 1e30f);
    std::vector<int>   parent(N, -1);
    std::vector<bool>  closed(N, false);

    // 最小堆：(f_cost, triangle_index)
    using NodePair = std::pair<float, int>;
    std::priority_queue<NodePair, std::vector<NodePair>, std::greater<NodePair>> openSet;

    auto heuristic = [&](int tri) -> float {
        const NCL::Maths::Vector3& c = m_Triangles[tri].centroid;
        const NCL::Maths::Vector3& g = m_Triangles[endTri].centroid;
        float dx = c.x - g.x, dy = c.y - g.y, dz = c.z - g.z;
        return sqrtf(dx*dx + dy*dy + dz*dz);
    };

    gCost[startTri] = 0.0f;
    openSet.push({ heuristic(startTri), startTri });

    while (!openSet.empty()) {
        auto [f, cur] = openSet.top();
        openSet.pop();

        if (closed[cur]) continue;
        closed[cur] = true;

        if (cur == endTri) {
            // 回溯路径
            outTriPath.clear();
            for (int i = endTri; i != -1; i = parent[i])
                outTriPath.push_back(i);
            std::reverse(outTriPath.begin(), outTriPath.end());
            return true;
        }

        for (int s = 0; s < 3; ++s) {
            int nb = m_Triangles[cur].neighbors[s];
            if (nb < 0 || closed[nb]) continue;

            const NCL::Maths::Vector3& cc = m_Triangles[cur].centroid;
            const NCL::Maths::Vector3& nc = m_Triangles[nb].centroid;
            float dx = nc.x - cc.x, dy = nc.y - cc.y, dz = nc.z - cc.z;
            float newG = gCost[cur] + sqrtf(dx*dx + dy*dy + dz*dz);

            if (newG < gCost[nb]) {
                gCost[nb]  = newG;
                parent[nb] = cur;
                openSet.push({ newG + heuristic(nb), nb });
            }
        }
    }

    return false; // 无法到达
}

// ============================================================
// FindPath — 公开接口（PathfinderUtil 实现）
// ============================================================
bool NavMeshPathfinderUtil::FindPath(const NCL::Maths::Vector3& start,
                                     const NCL::Maths::Vector3& end,
                                     std::vector<NCL::Maths::Vector3>& outPath)
{
    // navmesh 未加载：退化为直线追踪
    if (!m_Loaded || m_Triangles.empty()) {
        outPath.clear();
        outPath.push_back(end);
        return true;
    }

    int startTri = FindNearestTriangle(start);
    int endTri   = FindNearestTriangle(end);

    // 起终点在同一三角形：直接到目标
    if (startTri == endTri) {
        outPath.clear();
        outPath.push_back(end);
        return true;
    }

    std::vector<int> triPath;
    if (!AStarSearch(startTri, endTri, triPath)) {
        // A* 无解：退化为直线追踪
        LOG_WARN("[NavMeshPathfinderUtil] A* failed, falling back to straight line.");
        outPath.clear();
        outPath.push_back(end);
        return true;
    }

    // 将三角形序列转为路点（跳过 startTri，取后续三角形的重心）
    outPath.clear();
    for (int i = 1; i < static_cast<int>(triPath.size()); ++i) {
        outPath.push_back(m_Triangles[triPath[i]].centroid);
    }

    // 最后一个路点替换为精确目标位置
    if (!outPath.empty()) {
        outPath.back() = end;
    } else {
        outPath.push_back(end);
    }

    return true;
}

} // namespace ECS
