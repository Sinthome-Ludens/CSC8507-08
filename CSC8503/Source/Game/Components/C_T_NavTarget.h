#pragma once
#include <string>

namespace ECS {
    /**
     * @brief 通用导航目标标签
     * 任何带有此组件的实体都可以被 Pathfinder 识别为潜在目标
     */
    struct C_T_NavTarget {
        // 可以通过类型字符串区分目标，例如 "Player", "WayPoint", "Food"
        std::string targetType = "Player";
    };
}