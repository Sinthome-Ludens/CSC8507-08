#pragma once
#include <vector>
#include <string>
#include "Vector.h"

namespace ECS {
    struct C_D_NavAgent {
        float speed = 5.0f;
        float rotationSpeed = 10.0f;      ///< 转向速度（数值越大转得越快）
        bool smoothRotation = true;       ///< 是否开启平滑转向
        float stoppingDistance = 1.0f;
        float updateFrequency = 0.5f;
        float timer = 0.0f;

        // 新增：指定要追踪的目标类型标签
        std::string searchTag = "Player";

        std::vector<NCL::Maths::Vector3> currentPath;
        int currentWaypointIndex = 0;
        bool isActive = true;
    };
}