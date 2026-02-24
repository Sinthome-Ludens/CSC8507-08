#include "EnemyAI.h"

#include <algorithm> // std::clamp
#include <iostream>

// 内部调试输出宏，对齐项目 LOG_INFO 风格
#define AI_LOG(msg) std::cout << "[EnemyAI] " << msg << "\n"

// ============================================================
// Update — 帧更新主入口
// ============================================================
void EnemyAI::Update(float dt) {
    if (m_isDead) return;

    UpdateDetectionValue(dt);
    EvaluateState();
    ExecuteStateAction(dt);
    UpdateHacking(dt);
}

// ============================================================
// UpdateDetectionValue
// ============================================================
void EnemyAI::UpdateDetectionValue(float dt) {
    if (m_isSpotted) {
        m_detectionValue += DETECTION_INCREASE * dt;
    } else {
        m_detectionValue -= DETECTION_DECREASE * dt;
    }
    m_detectionValue = std::clamp(m_detectionValue, 0.0f, 100.0f);
}

// ============================================================
// EvaluateState
// ============================================================
void EnemyAI::EvaluateState() {
    m_previousState = m_currentState;

    if (m_detectionValue <= 15.0f) {
        m_currentState = EnemyState::Safe;
    } else if (m_detectionValue <= 30.0f) {
        m_currentState = EnemyState::Search;
    } else if (m_detectionValue <= 50.0f) {
        m_currentState = EnemyState::Alert;
    } else {
        m_currentState = EnemyState::Hunt;
    }

    m_stateChanged = (m_currentState != m_previousState);
    if (m_stateChanged) {
        const char* name = nullptr;
        switch (m_currentState) {
            case EnemyState::Safe:   name = "Safe";   break;
            case EnemyState::Search: name = "Search"; break;
            case EnemyState::Alert:  name = "Alert";  break;
            case EnemyState::Hunt:   name = "Hunt";   break;
        }
        AI_LOG("State -> " << name << " | detection=" << m_detectionValue);
    }
}

// ============================================================
// ExecuteStateAction
// ============================================================
void EnemyAI::ExecuteStateAction(float dt) {
    switch (m_currentState) {
        case EnemyState::Safe:   HandleSafeState();   break;
        case EnemyState::Search: HandleSearchState(); break;
        case EnemyState::Alert:  HandleAlertState();  break;
        case EnemyState::Hunt:   HandleHuntState(dt); break;
    }
    m_stateChanged = false; // 消费完毕
}

// ============================================================
// HandleSafeState — 原指令：MoveCommand::Patrol
// ============================================================
void EnemyAI::HandleSafeState() {
    m_huntLockTimer = 0.0f;

    if (m_stateChanged) {
        AI_LOG("Action: Patrol");
    }
}

// ============================================================
// HandleSearchState — 原指令：MoveCommand::Stop
// ============================================================
void EnemyAI::HandleSearchState() {
    m_huntLockTimer = 0.0f;

    if (m_stateChanged) {
        AI_LOG("Action: Stop — scanning surroundings");
    }
}

// ============================================================
// HandleAlertState — 原指令：MoveCommand::MoveToPosition 或 SweepRotate
// ============================================================
void EnemyAI::HandleAlertState() {
    m_huntLockTimer = 0.0f;

    if (m_stateChanged) {
        if (m_isSpotted) {
            AI_LOG("Action: MoveToPosition — moving towards player");
        } else {
            AI_LOG("Action: MoveToPosition — moving to last known position");
        }
    }
}

// ============================================================
// HandleHuntState — 原指令：MoveCommand::ChasePlayer（5 秒锁定期）
// ============================================================
void EnemyAI::HandleHuntState(float dt) {
    if (m_huntLockTimer > 0.0f) {
        m_huntLockTimer -= dt;

        if (m_stateChanged) {
            AI_LOG("Action: ChasePlayer — hunt lock active (" << HUNT_LOCK_DURATION << "s)");
        }
    } else {
        if (m_detectionValue > 50.0f) {
            m_huntLockTimer = HUNT_LOCK_DURATION;
            AI_LOG("Action: ChasePlayer — hunt lock reset");
        }
        // 否则 detectionValue 已降到阈值以下，EvaluateState 自动降级状态
    }
}

// ============================================================
// UpdateHacking
// ============================================================
void EnemyAI::UpdateHacking(float dt) {
    // 不在 Safe 状态时立即中断 Hack
    if (m_currentState != EnemyState::Safe) {
        if (m_inputEnabled || m_isHacked || m_hackProgress > 0.0f) {
            m_inputEnabled = false;
            m_hackProgress = 0.0f;
            m_isHacked     = false;
            AI_LOG("Hack interrupted — enemy no longer in Safe state");
        }
        return;
    }

    if (m_inputEnabled && m_hackKeyPressed) {
        if (m_hackProgress < 100.0f) {
            m_hackProgress += HACK_SPEED * dt;
        }

        if (m_hackProgress >= 100.0f && !m_isHacked) {
            m_hackProgress = 100.0f;
            m_isHacked     = true;
            AI_LOG("Hack complete — Execute is now available");
        }
    }
}

// ============================================================
// OnPlayerEnterKillzone
// ============================================================
void EnemyAI::OnPlayerEnterKillzone() {
    m_inKillzone = true;
    AI_LOG("Player Killzone entered enemy range");

    if (m_currentState == EnemyState::Safe) {
        m_inputEnabled = true;
        AI_LOG("Safe state confirmed — hack input enabled");
    } else {
        const char* name = nullptr;
        switch (m_currentState) {
            case EnemyState::Search: name = "Search"; break;
            case EnemyState::Alert:  name = "Alert";  break;
            case EnemyState::Hunt:   name = "Hunt";   break;
            default:                 name = "Unknown"; break;
        }
        AI_LOG("Not Safe (" << name << ") — hack input disabled");
    }
}

// ============================================================
// OnPlayerExitKillzone
// ============================================================
void EnemyAI::OnPlayerExitKillzone() {
    m_inKillzone   = false;
    m_inputEnabled = false;
    m_hackProgress = 0.0f;
    m_isHacked     = false;
    AI_LOG("Player left killzone — hack progress reset");
}

// ============================================================
// OnPlayerSpotted
// ============================================================
void EnemyAI::OnPlayerSpotted() {
    m_isSpotted = true;
    AI_LOG("Player spotted");
}

// ============================================================
// OnPlayerLost
// ============================================================
void EnemyAI::OnPlayerLost() {
    m_isSpotted = false;
    AI_LOG("Player lost");
}

// ============================================================
// TryExecute
// ============================================================
void EnemyAI::TryExecute() {
    if (m_isHacked && m_inputEnabled) {
        m_isDead = true;
        AI_LOG("Enemy executed and killed");
    }
}
