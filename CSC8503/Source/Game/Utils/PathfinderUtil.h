#pragma once
#include <vector>


namespace ECS {
    // 定义一个通用的路点数据，避免直接使用 NCL 类型
    struct NavWaypoint {
        float x, y, z;
    };

    class PathfinderUtil {
    public:
        virtual ~PathfinderUtil() = default;
        // 寻路接口，返回是否成功，并通过引用填充路径
        virtual bool FindPath(const NCL::Maths::Vector3& start, 
                             const NCL::Maths::Vector3& end, 
                             std::vector<NCL::Maths::Vector3>& outPath) = 0;
    };
}