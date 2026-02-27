#pragma once
#include <vector>
#include <string>
#include "Vector.h"
#include "Game/Components/Res_EnemyEnums.h"

namespace ECS {
    struct C_D_NavAgent {
        float speed = 5.0f;
        float rotationSpeed = 10.0f;      ///< 转向速度（数值越大转得越快）
        bool smoothRotation = true;       ///< 是否开启平滑转向
        float stoppingDistance = 1.0f;
        float updateFrequency = 0.5f;
        float timer = 0.0f;

        // 指定要追踪的目标类型标签
        std::string searchTag = "Player";

        std::vector<NCL::Maths::Vector3> currentPath;
        int currentWaypointIndex = 0;
        bool isActive = true;

        // ── 状态感知导航字段 ────────────────────────────────────────────────
        NCL::Maths::Vector3 lastKnownTargetPos{0.0f, 0.0f, 0.0f}; ///< 最后已知目标位置（Caution 旋转用）
        NCL::Maths::Vector3 alertSnapshotPos  {0.0f, 0.0f, 0.0f}; ///< 进入 Alert 时的位置快照
        EnemyState          prevState = EnemyState::Safe;           ///< 上一帧 AI 状态（用于检测首次进入 Alert）
        bool                hasLastKnownPos = false;                ///< 是否已记录过有效目标位置
    };
}