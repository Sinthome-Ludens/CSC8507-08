#pragma once

/**
 * @file EnemyAI.h
 * @brief 敌人 AI 状态机（独立逻辑类，无移动依赖）
 *
 * 从 enemyAI.cs 移植，移除所有 Unity/Movement 相关内容：
 *   - 已移除：MoveCommand 枚举、commandPosition、movementController、
 *             lastKnownPosition、ARRIVAL_DISTANCE
 *   - 替换：状态处理器中的移动指令改为控制台 debug 输出
 *
 * 状态阈值（detectionValue 范围）：
 *   Safe    0  ~ 15
 *   Search  16 ~ 30
 *   Alert   31 ~ 50
 *   Hunt    > 50
 *
 * 外部调用方式：
 *   每帧调用 Update(dt)；
 *   由碰撞系统调用 OnPlayer*() 事件接口；
 *   由输入系统调用 SetHackKeyPressed() / TryExecute()。
 */
class EnemyAI {
public:
    enum class EnemyState { Safe, Search, Alert, Hunt };

    EnemyAI() = default;

    // --- 主更新入口 ---
    void Update(float dt);

    // --- 碰撞 / 触发器事件（由外部物理系统在适当时机调用）---
    void OnPlayerEnterKillzone(); ///< 玩家 Killzone 进入敌人感知范围
    void OnPlayerExitKillzone();  ///< 玩家 Killzone 离开敌人感知范围
    void OnPlayerSpotted();       ///< 玩家身体进入视野触发器
    void OnPlayerLost();          ///< 玩家身体离开视野触发器

    // --- 输入事件 ---
    void SetHackKeyPressed(bool pressed) { m_hackKeyPressed = pressed; }
    void TryExecute(); ///< 玩家按下处决键时调用

    // --- 只读状态查询 ---
    EnemyState GetCurrentState()   const { return m_currentState; }
    float      GetDetectionValue() const { return m_detectionValue; }
    float      GetHackProgress()   const { return m_hackProgress; }
    bool       IsHacked()          const { return m_isHacked; }
    bool       IsDead()            const { return m_isDead; }

private:
    void UpdateDetectionValue(float dt);
    void EvaluateState();
    void ExecuteStateAction(float dt);
    void UpdateHacking(float dt);

    // 状态处理器（移动指令已替换为控制台输出）
    void HandleSafeState();
    void HandleSearchState();
    void HandleAlertState();
    void HandleHuntState(float dt);

    // --- 感知 ---
    float m_detectionValue = 0.0f;
    bool  m_isSpotted      = false;

    static constexpr float DETECTION_INCREASE = 10.0f;
    static constexpr float DETECTION_DECREASE =  5.0f;

    // --- 状态机 ---
    EnemyState m_currentState  = EnemyState::Safe;
    EnemyState m_previousState = EnemyState::Safe;
    bool       m_stateChanged  = false; ///< 标记本帧是否发生状态切换，由 EvaluateState 写入

    // --- Hunt 锁定期 ---
    float m_huntLockTimer = 0.0f;
    static constexpr float HUNT_LOCK_DURATION = 5.0f;

    // --- Hacking / Execute ---
    bool  m_inKillzone     = false;
    bool  m_hackKeyPressed = false;
    bool  m_inputEnabled   = false; ///< 当前是否允许 Hack 输入（在 Killzone 内且处于 Safe 状态）
    float m_hackProgress   = 0.0f;
    bool  m_isHacked       = false;
    bool  m_isDead         = false;

    static constexpr float HACK_SPEED = 25.0f;
};
