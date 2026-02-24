using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class enemyAI : MonoBehaviour
{
    // ========== 原有字段（保留） ==========
    public float detectionValue = 0.0f;
    public float detectionValueIncrease = 10.0f;
    public float detectionValueDecrease = 5.0f;
    private bool isSpotted = false;

    public bool HackEnble = false;
    public float progress = 0.0f;
    public float progressIncreaseValue = 25f;
    public bool IsHacked = false;
    private bool enbleInput = false;

    private bool Hackable = true;
    private GameObject _targetEnemy;


    private PlayerControls _controls;

[Header("处决设置")]
public GameObject spawnOnDeathPrefab; // 拖入你想要在处决后生成的预制体

    // ========== 移动指令输出系统 ==========
    public enum MoveCommand
    {
        Patrol,         // 巡逻
        Stop,           // 停止
        MoveToPosition, // 移动到指定位置
        ChasePlayer,    // 追击玩家
        SweepRotate     // 原地旋转 360°
    }

    public MoveCommand command = MoveCommand.Patrol;
    public Vector3 commandPosition = Vector3.zero;

    // ========== 状态机 ==========
    public enum EnemyState
    {
        Safe,    // ≤15
        Search,  // 16-30
        Alert,   // 31-50
        Hunt     // >50
    }

    private EnemyState currentState = EnemyState.Safe;
    private EnemyState previousState = EnemyState.Safe;

    // ========== 玩家追踪 ==========
    private Transform playerTransform;
    private Vector3 lastKnownPosition = Vector3.zero;

    // ========== Hunt 锁定期 ==========
    private float huntLockTimer = 0.0f;
    public float huntLockDuration = 5.0f;

    // ========== 距离判定 ==========
    private const float ARRIVAL_DISTANCE = 0.5f;

    // ========== 引用 enemyMovements ==========
    private enemyMovements movementController;

    void Start()
    {
        movementController = GetComponent<enemyMovements>();
        if (movementController == null)
        {
            Debug.LogError("enemyMovements not found on " + gameObject.name);
        }
    }

    void Awake()
    {
        _controls = new PlayerControls();

        // 动作名是 .Hack 和 .Execute
        _controls.player.Hack.started += ctx => HackEnble = true;
        _controls.player.Hack.canceled += ctx => HackEnble = false;

        _controls.player.Execute.performed += ctx => execute();

    }

    void Update()
    {
        // 1. 感知更新
        UpdatePerception();

        // 2. detectionValue 更新
        UpdateDetectionValue();

        // 3. 状态评估
        EvaluateState();

        // 4. 执行动作（输出指令）
        ExecuteStateAction();

        //击杀功能
        Hacking();
    }

    // ========== 1. 感知更新 ==========
    private void UpdatePerception()
    {
        if (isSpotted && playerTransform != null)
        {
            lastKnownPosition = playerTransform.position;

            // 更新 enemyMovements.target
            if (movementController != null)
            {
                movementController.target = playerTransform;
            }
        }
    }

    // ========== 2. detectionValue 更新 ==========
    private void UpdateDetectionValue()
    {
        if (isSpotted)
        {
            detectionValue += detectionValueIncrease * Time.deltaTime;
        }
        else
        {
            detectionValue -= detectionValueDecrease * Time.deltaTime;
        }

        // Clamp 到 [0, 100]
        detectionValue = Mathf.Clamp(detectionValue, 0.0f, 100.0f);
    }

    // ========== 3. 状态评估 ==========
    private void EvaluateState()
    {
        previousState = currentState;

        if (detectionValue <= 15)
        {
            currentState = EnemyState.Safe;
        }
        else if (detectionValue > 15 && detectionValue <= 30)
        {
            currentState = EnemyState.Search;
        }
        else if (detectionValue > 30 && detectionValue <= 50)
        {
            currentState = EnemyState.Alert;
        }
        else if (detectionValue > 50)
        {
            currentState = EnemyState.Hunt;
        }

        // 仅在状态切换时打印
        if (currentState != previousState)
        {
            Debug.Log("State -> " + currentState + " | detection=" + detectionValue.ToString("F1"));
        }
    }

    // ========== 4. 执行动作（输出指令） ==========
    private void ExecuteStateAction()
    {
        switch (currentState)
        {
            case EnemyState.Safe:
                HandleSafeState();
                break;
            case EnemyState.Search:
                HandleSearchState();
                break;
            case EnemyState.Alert:
                HandleAlertState();
                break;
            case EnemyState.Hunt:
                HandleHuntState();
                break;
        }
    }

    // ========== Safe 状态：巡逻 ==========
    private void HandleSafeState()
    {
        huntLockTimer = 0.0f;
        command = MoveCommand.Patrol;
    }

    // ========== Search 状态：固定不动 ==========
    private void HandleSearchState()
    {
        huntLockTimer = 0.0f;
        command = MoveCommand.Stop;
    }

    // ========== Alert 状态：追击或 Sweep ==========
    private void HandleAlertState()
    {
        huntLockTimer = 0.0f;

        // 检查是否到达 lastKnownPosition
        float distanceToLastKnown = Vector3.Distance(transform.position, lastKnownPosition);

        if (distanceToLastKnown <= ARRIVAL_DISTANCE)
        {
            // 到达：发出 Sweep 指令
            command = MoveCommand.SweepRotate;
        }
        else
        {
            // 未到达：移动到位置
            if (isSpotted && playerTransform != null)
            {
                // spotted：追 player.position
                commandPosition = playerTransform.position;
            }
            else
            {
                // not spotted：追 lastKnownPosition
                commandPosition = lastKnownPosition;
            }
            command = MoveCommand.MoveToPosition;
        }
    }

    // ========== Hunt 状态：5 秒锁定期 ==========
    private void HandleHuntState()
    {
        if (huntLockTimer > 0)
        {
            // 锁定期内：无条件追击玩家
            huntLockTimer -= Time.deltaTime;
            command = MoveCommand.ChasePlayer;
        }
        else
        {
            // 锁定期结束
            if (detectionValue > 50)
            {
                // 仍在 Hunt 阈值：重置锁定期
                huntLockTimer = huntLockDuration;
                command = MoveCommand.ChasePlayer;
            }
            // 否则按阈值降级（由 EvaluateState 自动处理）
        }
    }

    // ========== 触发器检测 ==========
    void OnTriggerEnter(Collider other)
    {

        if (other.CompareTag("Player"))
        {
            // 1. 优先检查：如果碰到的是名为 "Killzone" 的子触发器
            if (other.gameObject.name == "Killzone")
            {
                // [你的需求] 当 Killzone 与敌人重叠时输出 Debug
                Debug.Log("系统：检测到玩家的 [Killzone] 触发器已进入敌人碰撞范围！");
                playerTransform = other.transform.root; // 锁定玩家根对象坐标

                // 只有敌人处于 Safe 状态才允许获取暗杀输入权限
                if (currentState == EnemyState.Safe)
                {
                    enbleInput = true;
                    Debug.Log("safe,hackable");
                }
                else
                {
                    Debug.Log("Not safe(" + currentState + "),cant hack");
                }
            }
            else 
        {
            // 如果不是 Killzone 进入（即玩家身体接触），则触发正常的“被发现”逻辑
            isSpotted = true;
            playerTransform = other.transform;
            lastKnownPosition = playerTransform.position;

            if (movementController != null)
            {
                movementController.target = playerTransform;
            }
            Debug.Log("spotted");
        }


        }
        

    }

    void OnTriggerExit(Collider other)
    {
        if (other.CompareTag("Player"))
        {
            if (other.gameObject.name == "Killzone")
            {
                enbleInput = false;
                HackEnble = false;
                progress = 0;
                IsHacked = false;
                Debug.Log("not in rang , reset");
            }
            else
            {
                isSpotted = false;
            }
        }
    }

    // ========== Debug 可视化 ==========
    void OnDrawGizmosSelected()
    {
        // 绘制 lastKnownPosition
        Gizmos.color = Color.red;
        Gizmos.DrawWireSphere(lastKnownPosition, 0.5f);

        // 绘制当前指令目标位置
        if (command == MoveCommand.MoveToPosition)
        {
            Gizmos.color = Color.yellow;
            Gizmos.DrawLine(transform.position, commandPosition);
            Gizmos.DrawWireSphere(commandPosition, 0.3f);
        }
    }

    void Hacking()
    {
        // 实时检查：如果敌人不再安全，强制中断 Hack
        if (currentState != EnemyState.Safe)
        {
            enbleInput = false;
            HackEnble = false;
            progress = 0;
            IsHacked = false;
            return;
        }

        // 只有在范围内且按住 Hack 键时增加进度（删除了重复累加逻辑）
        if (enbleInput && HackEnble)
        {
            if (progress < 100)
            {
                progress += progressIncreaseValue * Time.deltaTime;
            }

            if (progress >= 100 && !IsHacked)
            {
                IsHacked = true;
                Debug.Log("Hack 完成，可以 Execute");
            }
        }
    }

    void execute()
    {
        // 如果进度满了，销毁自己
        if (IsHacked && enbleInput)
        {
            Debug.Log("killed");
            // [新增功能]：在当前敌人位置生成指定的预制体
        if (spawnOnDeathPrefab != null)
        {
            // 在敌人的位置 (transform.position) 和 旋转 (transform.rotation) 创建预制体
            Instantiate(spawnOnDeathPrefab, transform.position, transform.rotation);
        }
            Destroy(this.gameObject);
        }
    }
    // 4. Input System 生命周期管理 (容易漏掉)
    private void OnEnable() => _controls.Enable();  // 【必须添加】
    private void OnDisable() => _controls.Disable(); // 【必须添加】
}
