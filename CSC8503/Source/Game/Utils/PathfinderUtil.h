#pragma once
#include <vector>
#include "Vector.h"

namespace ECS {

/**
 * @brief 抽象寻路接口
 *
 * 通过接口隔离具体寻路算法，方便后续替换为 NavMesh A* 实现。
 * 注意：此工具类不是 ECS 组件，使用 std::vector 是合法的。
 */
class PathfinderUtil {
public:
    virtual ~PathfinderUtil() = default;

    /**
     * @brief 寻路接口，返回是否成功并通过引用填充路径
     * @param start  起始世界坐标
     * @param end    目标世界坐标
     * @param outPath 输出路径点列表（工具类级别，允许使用 std::vector）
     * @return 寻路成功则返回 true
     */
    virtual bool FindPath(const NCL::Maths::Vector3& start,
                          const NCL::Maths::Vector3& end,
                          std::vector<NCL::Maths::Vector3>& outPath) = 0;
};

} // namespace ECS
