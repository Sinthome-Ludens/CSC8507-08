#pragma once
#include "Game/Utils/PathfinderUtil.h"
#include <vector>

namespace ECS {
    /**
     * @brief 直接追踪寻路实现（单路点：直线到目标）
     *
     * 不依赖 NavMesh 三角形数据，始终生成 {end} 单路点路径。
     * Sys_Navigation 每 updateFrequency 秒调用一次，Enemy 方向持续更新跟随移动目标。
     *
     * 日后可升级为真正的 NavMesh A* 实现（替换 FindPath 内部逻辑即可，接口不变）。
     */
    class NavMeshPathfinderUtil : public PathfinderUtil {
    public:
        NavMeshPathfinderUtil() = default;

        bool FindPath(const NCL::Maths::Vector3& /*start*/,
                      const NCL::Maths::Vector3& end,
                      std::vector<NCL::Maths::Vector3>& outPath) override {
            outPath.clear();
            outPath.push_back(end);  // 直线追踪：目标位置即为唯一路点
            return true;
        }
    };
}
