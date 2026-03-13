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
// ComputeSlopeAngle — 计算三角形坡度角（0=平地，90=垂直墙）
// ============================================================
/** @brief 计算三角形法线与 Y 轴夹角（度），用于斜坡代价惩罚。 */
static float ComputeSlopeAngle(const NCL::Maths::Vector3& a,
                                const NCL::Maths::Vector3& b,
                                const NCL::Maths::Vector3& c)
{
    NCL::Maths::Vector3 ab(b.x - a.x, b.y - a.y, b.z - a.z);
    NCL::Maths::Vector3 ac(c.x - a.x, c.y - a.y, c.z - a.z);

    // 叉积：normal = ab × ac
    NCL::Maths::Vector3 normal(
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x);

    float len = sqrtf(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
    if (len < 0.0001f) return 0.0f;   // 退化三角形

    // cosθ = dot(normal, up) / |normal| = normal.y / len
    float cosTheta = normal.y / len;
    return acosf(std::clamp(cosTheta, -1.0f, 1.0f)) * 57.29577f;
}

// ============================================================
// LoadNavMesh — 检测格式并委托给对应解析器
// 命名格式（TutorialMap.navmesh）：首个非注释 token 为字母
// 纯数字格式（test.navmesh）：首个非注释 token 为整数
// ============================================================
/**
 * @brief 加载 .navmesh 文件，自动检测命名/纯数字格式并解析。
 * @param filePath .navmesh 文件绝对路径
 * @return true 加载成功，false 文件打不开或格式错误
 */
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

    // 探测格式：找到第一个有效行，看首 token 是否为纯数字
    std::string peek;
    while (std::getline(file, peek)) {
        if (!peek.empty() && peek[0] != '#') break;
    }

    std::istringstream ss(peek);
    int probe = 0;
    bool isRaw = (bool)(ss >> probe);   // 能解析成整数 → 纯数字格式

    // 回到文件开头
    file.clear();
    file.seekg(0, std::ios::beg);

    bool ok = isRaw ? LoadRawFormat(file) : LoadNamedFormat(file);
    if (ok) {
        m_Loaded = true;
        LOG_INFO("[NavMeshPathfinderUtil] Loaded navmesh: " << filePath
                 << " (" << m_Vertices.size() << " verts, "
                 << m_Triangles.size() << " tris)");
    }
    return ok;
}

// ============================================================
// LoadNamedFormat — 解析 TutorialMap.navmesh 风格文件
//
// 格式规范（以 TutorialMap.navmesh 为准）：
//   # 注释行（可选）
//   vertexCount N
//   indexCount  M        ← M = 总索引数，三角形数 = M/3
//   vertices
//     x y z  × N
//   indices
//     i1 i2 i3  × M/3   ← 每行一个三角形
//   areas              （可选节）
//     area_id  × M/3   ← 每行一个，0=可行走，1=不可通行
// ============================================================
/** @brief 解析命名格式 .navmesh（TutorialMap 等 Unity 导出文件）。 */
bool NavMeshPathfinderUtil::LoadNamedFormat(std::ifstream& file)
{
    int vertexCount = 0, indexCount = 0;

    // ── 1. 读取文件头 ────────────────────────────────────────
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        if      (token == "vertexCount") ss >> vertexCount;
        else if (token == "indexCount")  ss >> indexCount;
        else if (token == "vertices")    break;
    }

    if (vertexCount <= 0 || indexCount <= 0) {
        LOG_WARN("[NavMeshPathfinderUtil] Invalid header (named format).");
        return false;
    }

    if (indexCount % 3 != 0) {
        LOG_WARN("[NavMeshPathfinderUtil] indexCount=" << indexCount
                 << " is not a multiple of 3, truncating.");
    }

    const int triCount = indexCount / 3;

    // ── 2. 读取顶点 ──────────────────────────────────────────
    m_Vertices.reserve(vertexCount);
    int vRead = 0;
    while (vRead < vertexCount && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        float x, y, z;
        if (ss >> x >> y >> z) {
            m_Vertices.emplace_back(x, y, z);
            ++vRead;
        }
    }

    // ── 3. 跳到 indices 节 ───────────────────────────────────
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("indices") != std::string::npos) break;
    }

    // ── 4. 读取三角形（每行 i1 i2 i3）───────────────────────
    m_Triangles.reserve(triCount);
    int tRead = 0;
    while (tRead < triCount && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        NavTriangle tri;
        tri.neighbors[0] = tri.neighbors[1] = tri.neighbors[2] = -1;
        tri.centroid = NCL::Maths::Vector3(0, 0, 0);
        tri.area = 0;
        if (ss >> tri.v[0] >> tri.v[1] >> tri.v[2]) {
            m_Triangles.push_back(tri);
            ++tRead;
        }
    }

    if (vRead < vertexCount || tRead < triCount) {
        LOG_WARN("[NavMeshPathfinderUtil] Incomplete data (named format): "
                 << "verts=" << vRead << "/" << vertexCount
                 << " tris=" << tRead << "/" << triCount);
        return false;
    }

    // ── 5. 读取 areas 节（可选，每行一个 int）────────────────
    // 找到 areas 节（跳过其他内容）
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.find("areas") != std::string::npos) break;
    }
    // 尝试读取（若节不存在则 eof，不报错）
    int aRead = 0;
    while (aRead < triCount && std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        int areaId;
        if (ss >> areaId) {
            m_Triangles[aRead].area = areaId;
            ++aRead;
        }
    }
    // areas 节缺失时 aRead=0，各三角形保持默认 area=0（可行走）

    // ── 6. 建立邻接表 + 计算重心 ────────────────────────────
    BuildAdjacency();
    return true;
}

// ============================================================
// LoadRawFormat — 解析 test.navmesh 风格纯数字文件
//
// 格式：
//   vertexCount             ← 第 1 行
//   indexCount              ← 第 2 行（总索引数，三角形数=indexCount/3）
//   x y z  × vertexCount   ← 顶点（每行 3 个浮点数）
//   i × indexCount          ← 索引（逐个，每行 1 个整数；每 3 个构成一个三角形）
//   n0 n1 n2 × triCount     ← 邻居索引（每行 3 个，可选；-1=边界）
// ============================================================
/** @brief 解析纯数字格式 .navmesh（vertexCount indexCount 开头）。 */
bool NavMeshPathfinderUtil::LoadRawFormat(std::ifstream& file)
{
    int vertexCount = 0, indexCount = 0;
    if (!(file >> vertexCount >> indexCount) || vertexCount <= 0 || indexCount <= 0) {
        LOG_WARN("[NavMeshPathfinderUtil] Invalid header (raw format).");
        return false;
    }

    if (indexCount % 3 != 0) {
        LOG_WARN("[NavMeshPathfinderUtil] indexCount=" << indexCount
                 << " is not a multiple of 3, truncating.");
    }

    const int triCount = indexCount / 3;

    // ── 1. 读取顶点 ──────────────────────────────────────────
    m_Vertices.reserve(vertexCount);
    for (int i = 0; i < vertexCount; ++i) {
        float x, y, z;
        if (!(file >> x >> y >> z)) {
            LOG_WARN("[NavMeshPathfinderUtil] Vertex read error at " << i);
            return false;
        }
        m_Vertices.emplace_back(x, y, z);
    }

    // ── 2. 读取三角形索引（每 3 个构成一个三角形）───────────
    m_Triangles.resize(triCount);
    for (int i = 0; i < triCount; ++i) {
        NavTriangle& t = m_Triangles[i];
        t.neighbors[0] = t.neighbors[1] = t.neighbors[2] = -1;
        t.area = 0;
        if (!(file >> t.v[0] >> t.v[1] >> t.v[2])) {
            LOG_WARN("[NavMeshPathfinderUtil] Triangle read error at " << i);
            return false;
        }
        // 计算重心 + 坡度
        const auto& a = m_Vertices[t.v[0]];
        const auto& b = m_Vertices[t.v[1]];
        const auto& c = m_Vertices[t.v[2]];
        t.centroid.x = (a.x + b.x + c.x) / 3.0f;
        t.centroid.y = (a.y + b.y + c.y) / 3.0f;
        t.centroid.z = (a.z + b.z + c.z) / 3.0f;
        t.slope_angle = ComputeSlopeAngle(a, b, c);
    }

    // ── 3. 读取内嵌邻居数据（可选，每行 n0 n1 n2）──────────
    // 使用 peek 探测是否还有数据
    {
        int n0, n1, n2;
        for (int i = 0; i < triCount; ++i) {
            if (!(file >> n0 >> n1 >> n2)) break;   // 文件结束或格式不符则停止
            m_Triangles[i].neighbors[0] = n0;
            m_Triangles[i].neighbors[1] = n1;
            m_Triangles[i].neighbors[2] = n2;
        }
    }

    // 如果邻居仍全为 -1（文件无邻居节），则自动计算
    if (triCount > 0 && m_Triangles[0].neighbors[0] == -1 &&
                        m_Triangles[0].neighbors[1] == -1 &&
                        m_Triangles[0].neighbors[2] == -1)
    {
        BuildAdjacency();
    }

    return true;
}

// ============================================================
// ScaleVertices — 等比例缩放所有顶点坐标，并重新计算三角形重心
// ============================================================
/** @brief 等比例缩放所有顶点并重算重心与坡度角。 */
void NavMeshPathfinderUtil::ScaleVertices(float scale)
{
    for (auto& v : m_Vertices) {
        v.x *= scale;
        v.y *= scale;
        v.z *= scale;
    }
    for (auto& t : m_Triangles) {
        const auto& a = m_Vertices[t.v[0]];
        const auto& b = m_Vertices[t.v[1]];
        const auto& c = m_Vertices[t.v[2]];
        t.centroid.x = (a.x + b.x + c.x) / 3.0f;
        t.centroid.y = (a.y + b.y + c.y) / 3.0f;
        t.centroid.z = (a.z + b.z + c.z) / 3.0f;
        t.slope_angle = ComputeSlopeAngle(a, b, c);
    }
}

// ============================================================
// GetBoundaryEdges — 提取 navmesh 所有边界边（无邻居的三角形边）
//
// 原理：对每个三角形的每条边（v_e, v_{e+1 mod 3}），在所有其他三角形中
// 搜索是否存在坐标相同的对边（顺序任意）。若无，则该边是边界边（= 墙面）。
//
// 复杂度 O(N² × 9)，与 BuildAdjacency 相同，在场景加载时调用一次。
// ============================================================
/** @brief 提取所有无邻居的三角形边作为边界边（墙面位置），场景加载时调用。 */
std::vector<BoundaryEdge> NavMeshPathfinderUtil::GetBoundaryEdges() const
{
    std::vector<BoundaryEdge> result;
    if (!m_Loaded) return result;

    constexpr float kEps2 = 0.01f * 0.01f;
    int N = static_cast<int>(m_Triangles.size());

    auto posEq = [&](int vi, int vj) -> bool {
        const NCL::Maths::Vector3& a = m_Vertices[vi];
        const NCL::Maths::Vector3& b = m_Vertices[vj];
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx*dx + dy*dy + dz*dz < kEps2;
    };

    for (int i = 0; i < N; ++i) {
        const NavTriangle& ti = m_Triangles[i];

        for (int e = 0; e < 3; ++e) {
            int va = ti.v[e];
            int vb = ti.v[(e + 1) % 3];

            // 检查是否有其他三角形共享该边（正向或反向）
            bool shared = false;
            for (int j = 0; j < N && !shared; ++j) {
                if (j == i) continue;
                const NavTriangle& tj = m_Triangles[j];
                for (int f = 0; f < 3 && !shared; ++f) {
                    int vc = tj.v[f];
                    int vd = tj.v[(f + 1) % 3];
                    if ((posEq(va, vc) && posEq(vb, vd)) ||
                        (posEq(va, vd) && posEq(vb, vc))) {
                        shared = true;
                    }
                }
            }

            if (!shared) {
                const NCL::Maths::Vector3& p0 = m_Vertices[va];
                const NCL::Maths::Vector3& p1 = m_Vertices[vb];
                float dx  = p1.x - p0.x;
                float dz  = p1.z - p0.z;
                float len = sqrtf(dx*dx + dz*dz);
                if (len < 0.05f) continue;   // 跳过退化边

                BoundaryEdge be;
                be.v0       = p0;
                be.v1       = p1;
                be.midpoint = NCL::Maths::Vector3(
                    (p0.x + p1.x) * 0.5f,
                    (p0.y + p1.y) * 0.5f,
                    (p0.z + p1.z) * 0.5f);
                be.length   = len;
                be.dirX     = dx / len;
                be.dirZ     = dz / len;
                result.push_back(be);
            }
        }
    }

    return result;
}


//
// 使用 世界坐标位置比较（epsilon 容差） 而非顶点索引比较。
// 原因：Unity 导出的 navmesh 相邻三角形不共享整数索引，
//       而是重复写入相同坐标的顶点。
// 复杂度 O(N² × 9)，对 N ≤ 512 的 navmesh 足够快（< 2ms）。
// ============================================================
/** @brief 构建三角形邻接表（坐标 epsilon 比较），并计算坡度角。 */
void NavMeshPathfinderUtil::BuildAdjacency()
{
    int N = static_cast<int>(m_Triangles.size());

    // ── 计算重心 + 坡度 ──────────────────────────────────────────
    for (int i = 0; i < N; ++i) {
        NavTriangle& t = m_Triangles[i];
        const NCL::Maths::Vector3& a = m_Vertices[t.v[0]];
        const NCL::Maths::Vector3& b = m_Vertices[t.v[1]];
        const NCL::Maths::Vector3& c = m_Vertices[t.v[2]];
        t.centroid.x = (a.x + b.x + c.x) / 3.0f;
        t.centroid.y = (a.y + b.y + c.y) / 3.0f;
        t.centroid.z = (a.z + b.z + c.z) / 3.0f;
        t.slope_angle = ComputeSlopeAngle(a, b, c);
    }

    // ── 按位置判断是否相邻（共享两个坐标相同的顶点）────────────
    // eps²：顶点坐标差的平方和阈值（容差 0.01m）
    constexpr float kEps2 = 0.01f * 0.01f;

    auto vertexEqual = [&](int vi, int vj) -> bool {
        const NCL::Maths::Vector3& a = m_Vertices[vi];
        const NCL::Maths::Vector3& b = m_Vertices[vj];
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx*dx + dy*dy + dz*dz < kEps2;
    };

    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            int shared = 0;
            for (int a = 0; a < 3 && shared < 2; ++a)
                for (int b = 0; b < 3; ++b)
                    if (vertexEqual(m_Triangles[i].v[a], m_Triangles[j].v[b])) {
                        ++shared;
                        break;
                    }

            if (shared < 2) continue;

            for (int s = 0; s < 3; ++s) {
                if (m_Triangles[i].neighbors[s] == -1) {
                    m_Triangles[i].neighbors[s] = j;
                    break;
                }
            }
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
// FindNearestTriangle — 返回 3D 距离最近的可行走三角形（Y 权重 2×）
// Y 权重加倍可防止多层地图中选到错误楼层（垂直分离优先于水平接近）。
// 跳过 area != 0 的不可通行三角形。
// ============================================================
/** @brief 查找离给定点最近的可行走三角形，Y 方向权重 4×（平方后）。 */
int NavMeshPathfinderUtil::FindNearestTriangle(const NCL::Maths::Vector3& p) const
{
    int   best  = -1;
    float bestD = 1e30f;

    for (int i = 0; i < static_cast<int>(m_Triangles.size()); ++i) {
        if (m_Triangles[i].area != 0) continue;   // 跳过不可通行区域

        const NCL::Maths::Vector3& c = m_Triangles[i].centroid;
        float dx = p.x - c.x;
        float dy = p.y - c.y;
        float dz = p.z - c.z;
        float d  = dx*dx + (dy*dy)*4.0f + dz*dz;  // Y 权重 2×（平方后 4×）
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
/** @brief 三角形图 A* 搜索，斜坡代价 1.5×，输出三角形索引路径。 */
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
            if (m_Triangles[nb].area != 0) continue;   // 跳过不可通行区域

            const NCL::Maths::Vector3& cc = m_Triangles[cur].centroid;
            const NCL::Maths::Vector3& nc = m_Triangles[nb].centroid;
            float dx = nc.x - cc.x, dy = nc.y - cc.y, dz = nc.z - cc.z;
            float edgeDist = sqrtf(dx*dx + dy*dy + dz*dz);

            // 斜坡代价惩罚：坡度 > 15° 的三角形移动代价 × 1.5
            float slopePenalty = (m_Triangles[nb].slope_angle > 15.0f) ? 1.5f : 1.0f;
            float newG = gCost[cur] + edgeDist * slopePenalty;

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
// SharedEdgeMidpoint — 计算两个相邻三角形共享边的中点
// 用于生成"通过走廊中央"的自然路径路点，代替三角形重心。
// ============================================================
/** @brief 计算相邻三角形共享边的中点，用于生成自然路径路点。 */
static NCL::Maths::Vector3 SharedEdgeMidpoint(
    const NavTriangle& ti, const NavTriangle& tj,
    const std::vector<NCL::Maths::Vector3>& verts)
{
    constexpr float kEps2 = 0.01f * 0.01f;

    NCL::Maths::Vector3 sum(0, 0, 0);
    int found = 0;

    for (int a = 0; a < 3 && found < 2; ++a) {
        for (int b = 0; b < 3; ++b) {
            const NCL::Maths::Vector3& va = verts[ti.v[a]];
            const NCL::Maths::Vector3& vb = verts[tj.v[b]];
            float dx = va.x - vb.x, dy = va.y - vb.y, dz = va.z - vb.z;
            if (dx*dx + dy*dy + dz*dz < kEps2) {
                sum.x += va.x; sum.y += va.y; sum.z += va.z;
                ++found;
                break;
            }
        }
    }

    if (found == 0) return tj.centroid;   // 退化到重心（不应发生）
    return NCL::Maths::Vector3(sum.x / found, sum.y / found, sum.z / found);
}

// ============================================================
// FindPath — 公开接口（PathfinderUtil 实现）
// ============================================================
/**
 * @brief 计算 start→end 的 3D 导航路径（A* + 路径简化）。
 * @param start 起点世界坐标
 * @param end   终点世界坐标
 * @param outPath 输出简化后的路点列表
 * @return true 路径找到，false 起点/终点不在可行走区域或无通路
 */
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

    // 未找到可行走三角形（全部 area!=0 或 navmesh 为空）
    if (startTri < 0 || endTri < 0) {
        outPath.clear();
        outPath.push_back(end);
        return true;
    }

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

    // ── 将三角形路径转换为路点（共享边中点） ──────────────────
    // 路点 i 是从 triPath[i] 到 triPath[i+1] 的门口中点，
    // 比三角形重心更贴近实际通道中央，产生更自然的路径。
    outPath.clear();
    for (int i = 0; i + 1 < static_cast<int>(triPath.size()); ++i) {
        const NavTriangle& ta = m_Triangles[triPath[i]];
        const NavTriangle& tb = m_Triangles[triPath[i + 1]];
        outPath.push_back(SharedEdgeMidpoint(ta, tb, m_Vertices));
    }

    // 最后一个路点替换为精确目标位置
    if (!outPath.empty()) {
        outPath.back() = end;
    } else {
        outPath.push_back(end);
    }

    // ── 路径简化：移除近似共线路点（减少不必要的方向切换）──────────────
    // 若前→中→后三点在 XZ 平面上几乎共线（方向夹角 < ~11°），移除中间点。
    if (outPath.size() > 2) {
        std::vector<NCL::Maths::Vector3> simplified;
        simplified.push_back(outPath.front());
        for (size_t k = 1; k + 1 < outPath.size(); ++k) {
            const NCL::Maths::Vector3& prev = simplified.back();
            const NCL::Maths::Vector3& curr = outPath[k];
            const NCL::Maths::Vector3& next = outPath[k + 1];
            float ax = curr.x - prev.x, az = curr.z - prev.z;
            float bx = next.x - curr.x, bz = next.z - curr.z;
            float la = sqrtf(ax*ax + az*az);
            float lb = sqrtf(bx*bx + bz*bz);
            if (la < 0.01f || lb < 0.01f) continue; // 退化点跳过
            float dot = (ax/la)*(bx/lb) + (az/la)*(bz/lb);

            // 保留路点条件：XZ 方向变化 > 11° OR Y 高度变化 > 0.3m
            float yDelta = std::max(fabsf(curr.y - prev.y), fabsf(next.y - curr.y));
            if (dot < 0.98f || yDelta > 0.3f) {
                simplified.push_back(curr);
            }
        }
        simplified.push_back(outPath.back());
        outPath = std::move(simplified);
    }

    return true;
}

// ============================================================
// GetWalkableGeometry — 导出可行走三角形（供地板碰撞体生成）
// ============================================================
/** @brief 导出所有可行走三角形的顶点和索引，供 CreateNavMeshFloor 生成 Jolt MeshShape。 */
void NavMeshPathfinderUtil::GetWalkableGeometry(
    std::vector<NCL::Maths::Vector3>& outVerts,
    std::vector<int>& outIndices) const
{
    if (!m_Loaded) return;

    // 直接输出全部顶点（Jolt 仅使用索引引用的顶点）
    outVerts = m_Vertices;

    for (const auto& tri : m_Triangles) {
        if (tri.area != static_cast<int>(NavArea::Walkable)) continue;
        outIndices.push_back(tri.v[0]);
        outIndices.push_back(tri.v[1]);
        outIndices.push_back(tri.v[2]);
    }
}

} // namespace ECS
