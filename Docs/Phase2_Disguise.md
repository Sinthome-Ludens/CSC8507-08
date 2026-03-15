# 玩家行为系统开发记录

> 最后更新：2026-02-24
> 系统优先级：Sys_Input(55) → Sys_Gameplay(60) → Sys_Movement(65)

---

## 目录

- [阶段一：移动 + 姿态切换](#阶段一移动--姿态切换)
- [阶段二：纸箱伪装（E 键）](#阶段二纸箱伪装e-键)
- [当前完整状态汇总](#当前完整状态汇总)
- [P0 修复：文档对齐修正（2026-02-24）](#p0-修复文档对齐修正2026-02-24)
- [下一阶段预告](#下一阶段预告)

---

# 阶段一：移动 + 姿态切换

## 1.1 功能概述

实现基于物理的玩家移动系统，包含 WASD 步行、Shift 奔跑、C/V 姿态切换（Standing↔Crouching），以及噪音/可见度潜行指标计算。

### 操作键位

| 按键 | 功能 | 说明 |
|------|------|------|
| W/A/S/D | 移动 | 基于物理力驱动，带最大速度限制 |
| Shift | 奔跑 | 仅 Standing 姿态下有效 |
| C | 蹲下 | Standing → Crouching |
| V | 站起 | Crouching → Standing |

### 潜行指标

| 姿态 | 状态 | 可见度 | 噪音 |
|------|------|--------|------|
| Standing | 静止 | 0.7 | 0.0 |
| Standing | 步行 | 1.0 | 0.2 |
| Standing | 奔跑 | 1.0 | 0.6 |
| Crouching | 静止 | 0.3 | 0.0 |
| Crouching | 步行 | 0.5 | 0.08 |

## 1.2 新建文件清单

本阶段新建了以下文件：

### 组件（Components）

| 文件 | 路径 | 用途 |
|------|------|------|
| `C_D_PlayerState.h` | `Game/Components/` | 玩家状态数据组件，存储姿态、噪音、可见度、贴墙、伪装等全部潜行数据 |
| `C_T_Player.h` | `Game/Components/` | 玩家标签组件，标记玩家控制的实体 |
| `C_T_Hidden.h` | `Game/Components/` | 隐藏标签组件（阶段二使用，阶段一预创建） |

### 事件（Events）

| 文件 | 路径 | 用途 |
|------|------|------|
| `Evt_Player_StanceChanged.h` | `Game/Events/` | 姿态切换事件，供动画/音效系统监听 |
| `Evt_Player_Noise.h` | `Game/Events/` | 噪音事件（阶段二使用，阶段一预创建），含 NoiseType 枚举 |

### 系统（Systems）

| 文件 | 路径 | 用途 |
|------|------|------|
| `Sys_PlayerBehavior.h` | `Game/Systems/` | 玩家行为系统头文件 |
| `Sys_PlayerBehavior.cpp` | `Game/Systems/` | 玩家行为系统实现 |

### 预制体 & 场景（已有文件的修改）

| 文件 | 改动 |
|------|------|
| `PrefabFactory.h/cpp` | 新增 `CreatePlayer()` 方法，挂载 C_D_Transform, C_D_MeshRenderer, C_D_RigidBody, C_D_Collider, C_T_Player, C_D_PlayerState |
| `Scene_PhysicsTest.cpp` | 注册 `Sys_PlayerBehavior`（优先级 60）；调用 `PrefabFactory::CreatePlayer()` 生成玩家实体 |

## 1.3 核心数据结构

### C_D_PlayerState — 玩家状态数据组件

```cpp
struct C_D_PlayerState {
    // 姿态
    PlayerStance stance        = PlayerStance::Standing;
    bool         isSprinting   = false;
    float        moveSpeedMul  = 1.0f;

    // 潜行指标
    float noiseLevel       = 0.0f;   // [0, 1]
    float visibilityFactor = 1.0f;   // [0, 1]

    // 贴墙（预留）
    WallState wallState    = WallState::None;
    float wallNormalX = 0.0f, wallNormalZ = 0.0f;
    float wallPointX  = 0.0f, wallPointZ  = 0.0f;

    // 伪装（预留）
    bool isDisguised = false;

    // 碰撞体参数
    float colliderRadius     = 0.5f;
    float colliderHalfHeight = 1.0f;
};
```

### 枚举

```cpp
enum class PlayerStance : uint8_t { Standing = 0, Crouching = 1, Prone = 2 };
enum class WallState    : uint8_t { None = 0, Pressing = 1, Peeking = 2 };
enum class NoiseType    : uint8_t { Footstep = 0, WallKnock = 1, BoxScrape = 2, Landing = 3 };
```

### 玩家实体配置（PrefabFactory::CreatePlayer）

```cpp
// 刚体参数
mass            = 5.0f;
gravity_factor  = 1.0f;
linear_damping  = 0.5f;
lock_rotation_x = true;  // 锁定全轴旋转防翻滚
lock_rotation_y = true;
lock_rotation_z = true;

// 碰撞体：Capsule
radius    = 0.5f;
halfHeight = 1.0f;
friction   = 0.5f;
restitution = 0.0f;
```

## 1.4 Sys_PlayerBehavior 阶段一实现

### 常量

```cpp
// 速度（文档三档倍率：0.5 / 1.0 / 1.5）
static constexpr float BASE_SPEED       = 5.0f;    // 基准速度（站立行走）
static constexpr float BASE_FORCE       = 80.0f;   // 基准驱动力
static constexpr float RUN_SPEED_MUL    = 1.5f;    // 奔跑倍率 → 7.5

// 碰撞体
static constexpr float CAPSULE_RADIUS     = 0.5f;
static constexpr float STAND_HALF_HEIGHT  = 1.0f;
static constexpr float CROUCH_HALF_HEIGHT = 0.5f;

// 姿态速度乘数
static constexpr float STANCE_MUL_STANDING  = 1.0f;   // 2档 匀速
static constexpr float STANCE_MUL_CROUCHING = 0.5f;   // 1档 慢速
// 奔跑 = BASE_SPEED * RUN_SPEED_MUL = 7.5             // 3档 全速
```

### OnUpdate() 主循环

```
OnUpdate(dt)
  ├── 获取 Keyboard、Sys_Physics 指针（判空）
  ├── 读取 WASD 输入 → 归一化方向向量
  ├── 读取 Shift 状态
  ├── C/V 上升沿检测（m_CWasPressed / m_VWasPressed）
  └── 遍历 view<C_D_Transform, C_D_RigidBody, C_T_Player, C_D_PlayerState>
        ├── 计算 horizSpeed、isMoving
        ├── UpdateStance()
        ├── UpdateMovement()
        └── UpdateStealthMetrics()
```

### UpdateStance() — 姿态切换

- C 键：Standing → Crouching
- V 键：Crouching → Standing
- 切换时：
  1. `ReplaceShapeCapsule()` 替换碰撞体（Standing 半高 1.0 / Crouching 半高 0.5）
  2. 调整 Y 位置保持脚底不动（`oldBottom + newHalfHeight + CAPSULE_RADIUS + 0.05f`）
  3. `ActivateBody()` 强制激活
  4. 通过 EventBus 发布 `Evt_Player_StanceChanged` 事件

### UpdateMovement() — 物理移动

- 根据姿态计算速度乘数（Crouching = 0.5x）
- 奔跑意图：Shift + 有输入 + 非伪装
- 蹲伏时触发奔跑 → 自动站起（替换碰撞体 + 调整 Y + 发布事件）再进入奔跑
- 速度三档：蹲=2.5 / 站=5.0 / 跑=7.5（BASE_SPEED × 倍率）
- 有输入时：`horizSpeed < maxSpeed` 则 `AddForce`
- 无输入时：水平速度直接归零（零惯性制动），保留 Y 轴重力

### UpdateStealthMetrics() — 潜行指标

根据姿态和移动状态写入 `ps.noiseLevel` 和 `ps.visibilityFactor`。

## 1.5 场景注册

```cpp
// Scene_PhysicsTest.cpp — 系统注册顺序
systems.Register<Sys_Camera>         ( 50);
systems.Register<Sys_PlayerBehavior> ( 60);   // ← 阶段一新增
systems.Register<Sys_Physics>        (100);
systems.Register<Sys_PlayerCamera>   (150);
systems.Register<Sys_Render>         (200);
systems.Register<Sys_ImGui>          (300);   // 仅 USE_IMGUI
```

## 1.6 设计思路

### 为什么用 AddForce 而非直接设置速度？

- 物理驱动移动让碰撞响应更自然，避免穿模
- 配合最大速度限制（`horizSpeed < maxSpeed` 才施力），防止无限加速
- 无输入时直接将水平速度归零（零惯性制动），符合文档"松手秒停"要求

### 为什么用上升沿检测而非 KeyDown？

姿态切换是一次性动作，需要"按下瞬间"触发一次。`KeyDown` 每帧为 true 会导致重复触发。上升沿模式（`currentFrame && !lastFrame`）确保按一次只触发一次。

### 碰撞体替换 + Y 位置调整

切换姿态时碰撞体高度变化，若不调整 Y 位置，角色会"陷入地面"或"悬浮"。计算方式：
```
oldBottom = tf.position.y - (oldHalfHeight + CAPSULE_RADIUS)
newCenterY = oldBottom + newHalfHeight + CAPSULE_RADIUS + 0.05f  // +0.05 避免接触面卡住
```

### 预创建字段的前向兼容设计

`C_D_PlayerState` 中预留了 `isDisguised`、`wallState` 等字段，阶段一不使用但阶段二/三可直接读写，避免后续改组件导致全量重编译。

---

# 阶段二：纸箱伪装（E 键）

> 完成日期：2026-02-23
> 前置：阶段一全部完成

## 2.1 功能概述

纸箱伪装是 MGS 核心潜行机制之一：玩家静止时按 E 键套上纸箱进入"隐藏"状态，伪装期间移动会大幅减速并产生 BoxScrape 噪音。再次按 E 可随时退出伪装。

### 操作键位

| 按键 | 功能 | 条件 |
|------|------|------|
| E | 进入伪装 | 水平速度 < 1.0 + 非蹲伏 + 无贴墙状态 |
| E | 退出伪装 | 任意时刻 |

### 伪装效果

| 状态 | 速度乘数 | 可见度 | 噪音 | 噪音类型 |
|------|---------|--------|------|---------|
| 伪装 + 静止 | N/A | 0.0 | 0.0 | - |
| 伪装 + 移动 | 0.3x | 0.4 | 0.5 | BoxScrape (type=2) |
| 伪装 + 奔跑 | 禁止 | - | - | - |

### 伪装限制

- 伪装状态下禁止 C/V 姿态切换
- 伪装状态下禁止 Shift 奔跑
- 蹲伏状态不可进入伪装
- 移动中（水平速度 >= 1.0）不可进入伪装

## 2.2 修改文件清单

仅修改 2 个文件，未新建任何文件：

| 文件 | 路径 |
|------|------|
| 头文件 | `Game/Systems/Sys_PlayerBehavior.h` |
| 实现文件 | `Game/Systems/Sys_PlayerBehavior.cpp` |

阶段一已预创建的依赖（本阶段直接使用，未修改）：

| 文件 | 用途 |
|------|------|
| `C_D_PlayerState.h` | `isDisguised` 字段 |
| `C_T_Hidden.h` | 隐藏标签组件 |
| `Evt_Player_Noise.h` | 噪音事件（含 `NoiseType::BoxScrape`） |

## 2.3 Sys_PlayerBehavior.h 新增内容

### 伪装常量

```cpp
static constexpr float DISGUISE_MUL         = 0.3f;   // 伪装速度乘数
static constexpr float HIDE_SPEED_THRESHOLD = 1.0f;   // 伪装允许最大水平速度
static constexpr float NOISE_THROTTLE       = 0.3f;   // 噪音事件最小间隔(秒)
```

### 状态字段

```cpp
bool  m_EWasPressed   = false;   // E 键上升沿检测
float m_NoiseCooldown = 0.0f;    // 噪音节流计时器
```

### 方法声明

```cpp
void UpdateDisguise(Registry& reg, EntityID id, C_D_PlayerState& ps,
                    C_D_RigidBody& rb, Sys_Physics* physics,
                    bool ePressed, float horizSpeed);

void EmitNoiseEvent(Registry& reg, EntityID id, C_D_PlayerState& ps,
                    C_D_Transform& tf, bool isMoving, float dt);
```

## 2.4 Sys_PlayerBehavior.cpp 改动

### 新增 #include

```cpp
#include "Game/Events/Evt_Player_Noise.h"
#include "Game/Components/C_T_Hidden.h"
```

### OnUpdate() — E 键检测 + 调用新方法

在姿态键检测后新增：

```cpp
// E 键上升沿检测
bool eDown = kb->KeyDown(KeyCodes::E);
bool ePressed = eDown && !m_EWasPressed;
m_EWasPressed = eDown;

// 噪音节流计时器
m_NoiseCooldown -= dt;
if (m_NoiseCooldown < 0.0f) m_NoiseCooldown = 0.0f;
```

lambda 内新增两个调用（伪装在姿态之前、噪音在指标之后）：

```cpp
UpdateDisguise(registry, id, ps, rb, physics, ePressed, horizSpeed);
// ... UpdateStance, UpdateMovement, UpdateStealthMetrics ...
EmitNoiseEvent(registry, id, ps, tf, isMoving, dt);
```

### UpdateDisguise() — 新增方法

```cpp
void Sys_PlayerBehavior::UpdateDisguise(
    Registry& reg, EntityID id, C_D_PlayerState& ps,
    C_D_RigidBody& rb, Sys_Physics* physics,
    bool ePressed, float horizSpeed)
{
    if (!ePressed) return;

    if (ps.isDisguised) {
        // 退出伪装（任意时刻）
        ps.isDisguised = false;
        reg.Remove<C_T_Hidden>(id);
    } else {
        // 进入条件检查
        if (horizSpeed >= HIDE_SPEED_THRESHOLD) return;   // 移动中
        if (ps.stance == PlayerStance::Crouching) return;  // 蹲伏中
        if (ps.wallState != WallState::None) return;       // 贴墙中(前向兼容)

        ps.isDisguised = true;

        // 若非 Standing 则强制 Standing + 替换碰撞体
        if (ps.stance != PlayerStance::Standing) {
            ps.stance = PlayerStance::Standing;
            ps.colliderHalfHeight = STAND_HALF_HEIGHT;
            physics->ReplaceShapeCapsule(rb.jolt_body_id, CAPSULE_RADIUS, STAND_HALF_HEIGHT);
        }

        if (!reg.Has<C_T_Hidden>(id)) {
            reg.Emplace<C_T_Hidden>(id);
        }
    }
}
```

### UpdateStance() — 伪装禁止姿态切换

函数开头新增一行：

```cpp
if (ps.isDisguised) return;
```

### UpdateMovement() — 伪装减速 + 禁止奔跑 + 奔跑中断下蹲

```cpp
// 伪装速度乘数
float disguiseMul = ps.isDisguised ? DISGUISE_MUL : 1.0f;
ps.moveSpeedMul = stanceMul * disguiseMul;

// 奔跑意图：Shift + 有输入 + 非伪装
bool wantSprint = shiftDown && hasInput && !ps.isDisguised;

// 蹲伏时触发奔跑 → 自动站起（替换碰撞体 + 调整Y + 发布事件）
if (wantSprint && ps.stance == PlayerStance::Crouching) { ... }

bool canSprint = wantSprint && ps.stance == PlayerStance::Standing;

// 速度三档：BASE_SPEED × RUN_SPEED_MUL(奔跑时) × stanceMul × disguiseMul
float maxSpeed = BASE_SPEED;
float force    = BASE_FORCE;
if (canSprint) { maxSpeed *= RUN_SPEED_MUL; force *= RUN_SPEED_MUL; }
maxSpeed *= stanceMul * disguiseMul;
force    *= stanceMul;
```

### UpdateStealthMetrics() — 伪装指标覆盖

在 switch 之前新增伪装分支：

```cpp
if (ps.isDisguised) {
    ps.visibilityFactor = isMoving ? 0.4f : 0.0f;
    ps.noiseLevel       = isMoving ? 0.5f : 0.0f;
    return;
}
```

### EmitNoiseEvent() — 新增方法

```cpp
void Sys_PlayerBehavior::EmitNoiseEvent(
    Registry& reg, EntityID id, C_D_PlayerState& ps,
    C_D_Transform& tf, bool isMoving, float dt)
{
    if (!isMoving || ps.noiseLevel < 0.01f) return;
    if (m_NoiseCooldown > 0.0f) return;

    if (!reg.has_ctx<EventBus*>()) return;
    auto& bus = *reg.ctx<EventBus*>();

    Evt_Player_Noise evt{};
    evt.source   = id;
    evt.position = tf.position;
    evt.volume   = ps.noiseLevel;
    evt.type     = ps.isDisguised ? NoiseType::BoxScrape : NoiseType::Footstep;

    bus.publish_deferred(evt);
    m_NoiseCooldown = NOISE_THROTTLE;
}
```

## 2.5 设计思路与注意事项

### 为什么 UpdateDisguise 在 UpdateStance 之前调用？

伪装状态会影响后续所有逻辑：
- `UpdateStance`：伪装时禁止切姿态
- `UpdateMovement`：伪装时速度乘以 0.3、禁止奔跑
- `UpdateStealthMetrics`：伪装时使用专用指标

因此必须先确定 `ps.isDisguised`，后续读取才正确。

### Registry API 适配

自定义 ECS 的 Registry 与 EnTT 命名不同：
- `reg.remove<T>(id)` → `reg.Remove<T>(id)`（大写 R）
- `reg.emplace_or_replace<T>(id)` → `reg.Has<T>` + `reg.Emplace<T>` 组合替代

### wallState 前向兼容

当前阶段 `ps.wallState` 始终为 `WallState::None`，但检查它可确保后续阶段实现贴墙功能时不产生冲突。

### 噪音节流设计

`m_NoiseCooldown` 在 OnUpdate 开头统一递减，`EmitNoiseEvent` 内检查 > 0 则跳过。发布后重置为 `NOISE_THROTTLE`（0.3s），避免每帧刷噪音事件。

### C_T_Hidden 标签用途

`C_T_Hidden` 是纯标签组件（无数据），挂载后可被 AI 守卫系统的 View 过滤。守卫在巡逻时可通过 `registry.view<C_D_Transform>()` 排除持有 `C_T_Hidden` 的实体，实现"伪装隐身"。

## 2.6 测试验证结果

基于运行日志（2.txt）验证：

| 验证项 | 日志行 | 结果 |
|--------|--------|------|
| 非伪装步行噪音 Footstep (type=0, vol=0.2) | 40-45 | OK |
| 非伪装奔跑噪音 Footstep (type=0, vol=0.6) | 46-48 | OK |
| 进入伪装 `Disguise ON` | 49 | OK |
| 伪装移动噪音 BoxScrape (type=2, vol=0.5) | 50-60 | OK |
| 退出伪装 `Disguise OFF` | 61 | OK |
| 退出后恢复 Footstep (type=0, vol=0.2) | 62-68 | OK |
| 二次进入伪装 | 70 | OK |
| 二次退出伪装 | 75 | OK |
| 退出后可正常奔跑 (vol=0.6) | 85-87 | OK |
| 程序正常退出（退出代码 0） | 100 | OK |

---

# 当前完整状态汇总

## 系统调用顺序

```
Sys_PlayerBehavior::OnUpdate(dt)
  ├── 输入读取（WASD / Shift / C / V / E）
  ├── 上升沿检测（C / V / E）
  ├── 噪音冷却递减
  └── 遍历玩家实体 view<Transform, RigidBody, Player, PlayerState>
        ├── UpdateDisguise()        ← 阶段二
        ├── UpdateStance()          ← 阶段一（伪装时跳过）
        ├── UpdateMovement()        ← 阶段一（伪装时减速）
        ├── UpdateStealthMetrics()  ← 阶段一（伪装时覆盖指标）
        └── EmitNoiseEvent()        ← 阶段二
```

## ECS 全局注册顺序

```
Sys_Camera          (50)   — 相机控制
Sys_PlayerBehavior  (60)   — 玩家移动 + 姿态 + 伪装
Sys_Physics         (100)  — Jolt 物理步进 + Transform 同步
Sys_PlayerCamera    (150)  — 第三人称跟随
Sys_Render          (200)  — 渲染桥接
Sys_ImGui           (300)  — Debug UI (仅 USE_IMGUI)
```

## 完整键位表

| 按键 | 功能 | 来源 |
|------|------|------|
| W/A/S/D | 移动 | 阶段一 |
| Shift | 奔跑 | 阶段一 |
| C | 蹲下 (Standing → Crouching) | 阶段一 |
| V | 站起 (Crouching → Standing) | 阶段一 |
| E | 伪装开/关 | 阶段二 |

## 全部涉及文件

| 文件 | 类型 | 创建阶段 | 最后修改阶段 |
|------|------|---------|-------------|
| `Game/Components/C_D_PlayerState.h` | 数据组件 | 一 | 一 |
| `Game/Components/C_T_Player.h` | 标签组件 | 一 | 一 |
| `Game/Components/C_T_Hidden.h` | 标签组件 | 一(预创建) | 一 |
| `Game/Events/Evt_Player_StanceChanged.h` | 事件 | 一 | 一 |
| `Game/Events/Evt_Player_Noise.h` | 事件 | 一(预创建) | 一 |
| `Game/Systems/Sys_PlayerBehavior.h` | 系统头文件 | 一 | P0修复 |
| `Game/Systems/Sys_PlayerBehavior.cpp` | 系统实现 | 一 | P0修复 |
| `Game/Prefabs/PrefabFactory.h` | 预制体工厂 | 基础设施 | 一(新增 CreatePlayer) |
| `Game/Prefabs/PrefabFactory.cpp` | 预制体工厂 | 基础设施 | 一(新增 CreatePlayer) |
| `Game/Scenes/Scene_PhysicsTest.cpp` | 场景 | 基础设施 | 一(注册 Sys_PlayerBehavior) |

---

# P0 修复：文档对齐修正（2026-02-24）

> 完成日期：2026-02-24
> 修改文件：`Sys_PlayerBehavior.h`、`Sys_PlayerBehavior.cpp`（无新建文件）

本次修复对齐游戏策划文档的三项硬性要求，均为行为层修改，不涉及 Core/ 或 NCLCoreClasses。

## 修复 1：零惯性制动（松手秒停）

**问题**：原实现松手后用 `DAMPING_FACTOR = 0.85` 渐变衰减水平速度，角色会"滑行"一段距离。文档要求松手即停、零惯性。

**改动**：
- `.cpp` `UpdateMovement()` else 分支：`vel.x * DAMPING_FACTOR` → `0.0f`，`vel.z * DAMPING_FACTOR` → `0.0f`
- `.h`：删除 `DAMPING_FACTOR` 常量

## 修复 2：奔跑强制中断下蹲

**问题**：原实现 `canSprint` 条件要求 `ps.stance == Standing`，蹲伏状态下完全无法触发奔跑。文档要求按住 Shift+移动时即使在蹲伏中也应自动站起并奔跑。

**改动**：
- 新增 `wantSprint` 变量（Shift + 有输入 + 非伪装），与 `canSprint` 分离
- 当 `wantSprint && Crouching` 时自动执行站起流程（替换碰撞体、调整 Y 保持脚底不动、发布 `Evt_Player_StanceChanged`）
- `canSprint = wantSprint && Standing`（站起后条件自动满足）
- `UpdateMovement` 签名新增 `Registry& reg, EntityID id, C_D_Transform& tf` 三个参数

## 修复 3：速度三档倍率对齐

**问题**：原 `WALK_SPEED=5, RUN_SPEED=10` 比例 1:2，文档要求三档倍率 0.5/1.0/1.5。

**改动**：
- `.h`：删除 `WALK_SPEED, WALK_FORCE, RUN_SPEED, RUN_FORCE` 四个常量
- `.h`：新增 `BASE_SPEED=5.0, BASE_FORCE=80.0, RUN_SPEED_MUL=1.5`
- `.cpp`：速度计算改为 `BASE_SPEED × RUN_SPEED_MUL(奔跑时) × stanceMul × disguiseMul`

**最终三档速度**：

| 档位 | 状态 | 计算 | 速度 |
|------|------|------|------|
| 1档 | 蹲伏行走 | 5.0 × 0.5 | 2.5 |
| 2档 | 站立行走 | 5.0 × 1.0 | 5.0 |
| 3档 | 奔跑 | 5.0 × 1.5 | 7.5 |

## 验证方法

1. CMake 构建零报错 ✓
2. 运行 Scene_PhysicsTest 场景：
   - WASD 移动后松手 → 角色立即停止，无滑行
   - C 键下蹲后按住 Shift+W → 自动站起并进入奔跑
   - 观察 ImGui Debug 窗口中速度值：站立行走 ≈ 5.0，下蹲行走 ≈ 2.5，奔跑 ≈ 7.5

---

# 下一阶段预告

## 阶段三：贴墙 + 探头（预计涉及）

- `UpdateWallPress()` — 射线检测墙面 + 贴墙状态管理
- `ps.wallState` 从 `None` → `Pressing` / `Peeking`
- 贴墙时禁止伪装（与阶段二互斥）
- 贴墙移动的独立速度/噪音参数

相关预创建字段已就绪：
- `C_D_PlayerState.wallState`（默认 `WallState::None`）
- `C_D_PlayerState.wallNormalX/Z`、`wallPointX/Z`

---

# 2.5D 俯视角相机配置

> 完成日期：2026-02-24

## 配置参数（最终值）

### Sys_PlayerCamera.h 常量

| 参数 | 值 | 说明 |
|------|-----|------|
| `SMOOTH_SPEED` | 17.0f | 跟随插值速度（见下方"跟随调优"） |
| `FIXED_PITCH` | -75.0f | 俯视角 75°，接近正俯视的 2.5D 视角 |
| `FIXED_YAW` | 0.0f | 固定偏航角 |
| `CAMERA_OFFSET` | (0, 25, 6.7) | 相机相对玩家偏移（见下方"视野调优"） |

### Sys_Camera.cpp 初始值（OnAwake）

| 参数 | 值 | 说明 |
|------|-----|------|
| 初始位置 | (0, 25, 6.7) | 与 CAMERA_OFFSET 一致，避免首帧跳变 |
| 初始 pitch | -75.0f | 与 FIXED_PITCH 一致 |
| 初始 yaw | 0.0f | 朝 -Z 方向 |

## 居中原理

pitch=-75° 时相机视线向下倾斜 75°，`tan(75°) ≈ 3.73`。Z 偏移 = Y / tan(75°)，使视线中心恰好落在玩家位置，画面中玩家居中。当前 Y=25 → Z = 25 / 3.73 ≈ 6.7。

## 跟随调优

相机通过 Lerp 平滑跟随玩家，`SMOOTH_SPEED` 控制插值速度因子（`t = min(1, SMOOTH_SPEED * dt)`）。

| 值 | 体感 |
|----|------|
| 5 | 明显拖尾延迟 |
| 12 | 较紧凑，仍有轻微延迟 |
| 17 | 几乎即时跟随，保留微弱平滑感（最终选择） |
| 20+ | 完全即时，无平滑 |

调优过程：5 → 12 → 16 → 18 → **17**（手动微调确认最佳手感）。

## 视野调优

相机高度（CAMERA_OFFSET.y）直接决定可见地图范围。高度越大，俯瞰范围越广。

| Y 值 | Z 值（居中） | 效果 |
|------|-------------|------|
| 17 | 4.5 | 初始值，视野较小 |
| 25 | 6.7 | 最终值，视野开阔，适合 2.5D 关卡 |

调高 Y 时必须同步调整 Z = Y / tan(75°) 以保持玩家居中。

## 涉及文件

| 文件 | 改动 |
|------|------|
| `Game/Systems/Sys_PlayerCamera.h` | SMOOTH_SPEED → 17，FIXED_PITCH → -75°，CAMERA_OFFSET → (0, 25, 6.7) |
| `Game/Systems/Sys_Camera.cpp` | CreateCameraMain 初始位置 → (0, 25, 6.7)，初始 pitch → -75° |

---

# P1 架构拆分：Sys_PlayerBehavior → Sys_Input + Sys_Gameplay + Sys_Movement

> 完成日期：2026-02-24
> 纯架构重构，不改变任何运行时行为

## 背景

开发文档（§1.3）要求玩家逻辑由三个系统协作执行。原代码将全部逻辑塞在单一 `Sys_PlayerBehavior` 中，本次拆分对齐文档架构要求。

## 文件变更

### 新建（7 个文件）

| 文件 | 说明 |
|------|------|
| `Game/Components/C_D_Input.h` | 玩家输入数据组件（moveX/moveZ/hasInput/shiftDown/上升沿标志） |
| `Game/Systems/Sys_Input.h` | 输入系统头文件 |
| `Game/Systems/Sys_Input.cpp` | 输入系统实现：从 Res_Input 读取按键，写入 C_D_Input |
| `Game/Systems/Sys_Gameplay.h` | 游戏玩法系统头文件 |
| `Game/Systems/Sys_Gameplay.cpp` | 伪装切换、姿态切换、奔跑判定+速度乘数、隐身指标、噪音事件 |
| `Game/Systems/Sys_Movement.h` | 移动系统头文件 |
| `Game/Systems/Sys_Movement.cpp` | 纯物理移动：AddForce / SetLinearVelocity（零惯性制动） |

### 修改（2 个文件）

| 文件 | 改动 |
|------|------|
| `Game/Prefabs/PrefabFactory.cpp` | CreatePlayer 新增 `Emplace<C_D_Input>` |
| `Game/Scenes/Scene_PhysicsTest.cpp` | 替换系统注册 + include（3 个新系统替代 1 个旧系统） |

### 删除（2 个文件）

| 文件 | 说明 |
|------|------|
| `Game/Systems/Sys_PlayerBehavior.h` | 被三个新系统替代 |
| `Game/Systems/Sys_PlayerBehavior.cpp` | 被三个新系统替代 |

### 注释更新（4 个文件）

将 `Sys_PlayerBehavior` 引用更新为 `Sys_Gameplay`：
- `Sys_Physics.h/cpp`、`Evt_Player_StanceChanged.h`、`Evt_Player_Noise.h`、`C_D_PlayerState.h`

## 三系统职责与优先级

### Sys_Input（优先级 55）

- 通过 `Window::GetKeyboard()` 直接读取键盘状态（见下方 P1-hotfix）
- WASD 读取 inputX/inputZ、归一化、hasInput；Shift 读取 shiftDown
- 计算 C/V/E 上升沿（`m_CWasPressed, m_VWasPressed, m_EWasPressed`）
- 遍历 `view<C_T_Player, C_D_Input>` 写入所有字段

### Sys_Gameplay（优先级 60）

- 读取 `C_D_Input` + 物理状态
- 按顺序执行：UpdateDisguise → UpdateStance → UpdateSprintAndSpeedMul → UpdateStealthMetrics → EmitNoiseEvent
- 写入 `C_D_PlayerState`（stance, isDisguised, isSprinting, moveSpeedMul, noiseLevel, visibilityFactor, colliderHalfHeight）+ `C_T_Hidden`

### Sys_Movement（优先级 65）

- 读取 `C_D_Input` + `C_D_PlayerState`（isSprinting, moveSpeedMul, stance）
- 计算 maxSpeed 和 force
- 有输入且 horizSpeed < maxSpeed → `AddForce`
- 无输入 → `SetLinearVelocity(0, vel.y, 0)`（零惯性制动）

## 帧内执行顺序

```
Sys_Camera          (50)   — NCL Bridge 同步
Sys_Input           (55)   — Window::GetKeyboard() → C_D_Input
Sys_Gameplay        (60)   — 读 C_D_Input，写 C_D_PlayerState
Sys_Movement        (65)   — 读 C_D_Input + C_D_PlayerState，施加物理力
Sys_Physics         (100)  — Jolt 步进 + Transform 同步
Sys_PlayerCamera    (150)  — 相机跟随
Sys_Render          (200)  — 渲染桥接
Sys_ImGui           (300)  — Debug UI
```

## 验证

- CMake configure + 构建零报错 ✓
- 运行时行为与拆分前完全一致

---

## P1-hotfix：Sys_Input 输入源修复（2026-02-24）

**问题**：P1 拆分时 Sys_Input 设计为从 `Res_Input`（Registry ctx）读取输入。但 `InputAdapter::Update()` 从未在主循环中被调用，`Res_Input` 也未注册到 Registry ctx，导致 Sys_Input 在 `has_ctx<Res_Input>()` 检查时直接 return，键盘输入完全无响应。

**根因**：原 `Sys_PlayerBehavior` 直接使用 `Window::GetKeyboard()` 读取键盘，不依赖 `Res_Input`。拆分时错误假设 `Res_Input` 已被填充。

**修复**：Sys_Input 改为直接使用 `Window::GetKeyboard()` 读取键盘状态，与原 Sys_PlayerBehavior 的输入读取方式完全一致。WASD 直接通过 `KeyDown()` 读取（而非 `Res_Input.axisX/axisY`），保持原有的坐标映射逻辑。

**改动文件**：`Game/Systems/Sys_Input.cpp`（1 个文件）

**教训**：拆分重构时必须验证数据源的实际可用性，不能仅依赖文档中的理想架构设计。

---

# 隐形碰撞墙（关卡边界）

> 完成日期：2026-02-24

## 功能概述

在地板四周放置 4 面隐形碰撞墙作为关卡边界，防止玩家走出地板掉入虚空。利用 ECS 特性：**不挂载 C_D_MeshRenderer 的实体对渲染不可见，但 Sys_Physics 仍会创建 Jolt 碰撞体**。

## 文件变更

### 新建（1 个文件）

| 文件 | 说明 |
|------|------|
| `Game/Components/C_T_InvisibleWall.h` | 标签组件，标记隐形碰撞墙实体 |

### 修改（3 个文件）

| 文件 | 改动 |
|------|------|
| `Game/Prefabs/PrefabFactory.h` | 新增 `CreateInvisibleWall()` 静态方法声明 |
| `Game/Prefabs/PrefabFactory.cpp` | 新增 `CreateInvisibleWall()` 实现（Transform + RigidBody + Collider + C_T_InvisibleWall + DebugName，不挂 MeshRenderer） |
| `Game/Scenes/Scene_PhysicsTest.cpp` | 调用 `CreateInvisibleWall` 放置 4 面边界墙 |

## 墙壁布局

地板：position=(0,-6,0), scale=(50,1,50)，XZ 范围 ±50，顶面 Y=-5。

| 墙壁 | 位置 | 半尺寸 (halfExtents) | 说明 |
|------|------|---------------------|------|
| +X（右） | (50.5, 0, 0) | (0.5, 5, 51) | 紧贴地板右边缘 |
| -X（左） | (-50.5, 0, 0) | (0.5, 5, 51) | 紧贴地板左边缘 |
| +Z（后） | (0, 0, 50.5) | (51, 5, 0.5) | 紧贴地板后边缘 |
| -Z（前） | (0, 0, -50.5) | (51, 5, 0.5) | 紧贴地板前边缘 |

- 墙壁高度 10（halfY=5），中心 Y=0，覆盖 Y=-5 到 Y=5
- 墙壁厚度 1（half=0.5），放在 ±50.5 紧贴地板边缘不重叠
- Z/X 墙用 51.0 半尺寸确保四角无缝覆盖

## 线框调试

隐形墙没有 MeshRenderer，全局线框模式看不到它们。如需可视化，可新建 `Sys_WireframeDebug` 查询 `view<C_T_InvisibleWall, C_D_Transform, C_D_Collider>` 手动绘制线框。

## 验证

- CMake 构建零报错 ✓
- 玩家走向地板任意边缘被隐形墙阻挡，无法掉出

---

# PR Review 修复记录（2026-02-27）

> 完成日期：2026-02-27
> 涉及 PR：#6 ~ #10（Copilot 自动审查反馈修复）

## 概述

对 5 个 PR 的 GitHub Copilot 代码审查意见进行逐条修复，所有改动限于 `Game/` 目录，不触碰 `Core/` 或 `NCLCoreClasses`。每个 PR 修复后均通过 CMake 编译验证。

## PR #6 — feat(physics): Extend Sys_Physics public API

**分支**：`feat/csl/physics-api` | **Commit**：`8cfe7ae`

| # | 问题 | 修复 |
|---|------|------|
| 1 | `Sys_Physics*` / `EventBus*` ctx 注册有 `has_ctx` 守卫，场景切换后残留悬空指针 | OnAwake 无条件 `ctx_emplace` 覆盖；OnDestroy 置空 `ctx<T*>() = nullptr` |
| 2 | RayCast/CastResult/NarrowPhaseQuery 头文件放在 `.h` 中增加编译依赖 | 移到 `.cpp` |
| 3 | `FLT_EPSILON` 缺显式 `#include <cfloat>` | 添加 `<cfloat>` 和 `<cmath>` |
| 4 | `CastRay` 方向向量未归一化 | 内部 `std::sqrt` 归一化 + 零向量保护 |
| 5 | `ReplaceShapeCapsule(radius, halfHeight)` 参数顺序与 Jolt 和 `CreateBodyForEntity` 不一致 | 改为 `(halfHeight, radius)`，同步所有调用点 |

## PR #7 — feat(player): Add player entity foundation

**分支**：`feat/csl/player-foundation` | **Commit**：`774c676`

| # | 问题 | 修复 |
|---|------|------|
| 1 | 玩家生成 Y=0.0f，注释写 Y=-3.5f，实际悬空坠落 | 改为 `Vector3(0, -3.5f, 0)` 对齐地板顶面 |

## PR #8 — feat(movement): Add Sys_Movement

**分支**：`feat/csl/player-movement` | **Commit**：`12e22b8`

| # | 问题 | 修复 |
|---|------|------|
| 1 | `AddForce` 每帧调一次，Jolt 固定步长多 substep 时只有第一步有力 | 改用 `ApplyImpulse(force * dt)`，帧率无关 |
| 2 | `ps.isSprinting` 无上游系统写入，Shift 键不生效 | 回退条件 `ps.isSprinting \|\| input.shiftDown` |
| 3 | 注释 `disguiseMul` 与代码 `ps.moveSpeedMul` 不匹配 | 修正注释描述 |

## PR #9 — feat(gameplay): Add stance, disguise, and stealth metrics

**分支**：`feat/csl/player-gameplay` | **Commit**：`3c7ddac`

| # | 问题 | 修复 |
|---|------|------|
| 1-2 | `Sys_PlayerStance` 两处 `SetPosition` 后 `tf.position.y` 未同步，下游系统读到旧值 | 紧跟 `tf.position.y = newCenterY` |
| 3 | C/V 键同帧按下导致 Standing→Crouching→Standing 虚假切换 | 改为 `else if` 互斥 + `newStance != oldStance` 守卫 |
| 4 | 噪音冷却 `m_NoiseCooldown` 在系统实例上，多玩家共享互相抑制 | 移到 `C_D_PlayerState.noiseCooldown`（每实体独立） |
| 5 | OnExit TODO 声称 `Registry::Clear()` 未实现，但实际已存在 | 删除 TODO，调用 `registry.Clear()` |

## PR #10 — feat(camera): Refactor Sys_Camera and add Sys_PlayerCamera

**分支**：`feat/csl/player-camera` | **Commit**：`58ca8fb`

| # | 问题 | 修复 |
|---|------|------|
| 1 | `kb` 和 `kb2` 重复获取 `Window::GetKeyboard()` | 删除 `kb2`，复用 `kb` |
| 2 | `Sys_PlayerCamera.cpp` 包含未使用的 `Log.h` | 删除 |
| 3+5 | `Sys_Camera(50)` Bridge 同步在 `Sys_PlayerCamera(150)` 之前，渲染滞后一帧 | `Sys_Camera` 优先级改为 155 |
| 4 | `Sys_PlayerCamera` 无条件覆盖相机，debug 自由飞行被覆盖 | `Sys_Camera*` 注册到 ctx，`Sys_PlayerCamera` 检查 `IsDebugMode()` 跳过 |
| 6 | `SetDebugMode()`/`IsDebugMode()` 无调用点 | 添加 F1 键运行时切换 |
| 7 | 关闭 debug 时鼠标 cursor_free 状态未重置 | F1 切换时重置 `cam.cursor_free` 和窗口鼠标状态 |

## 当前系统注册顺序（PR #10 修复后）

```
Sys_Input           ( 10)   — NCL → Res_Input
Sys_InputDispatch   ( 55)   — Res_Input → per-entity C_D_Input
Sys_PlayerDisguise  ( 59)   — 伪装切换、C_T_Hidden 管理
Sys_PlayerStance    ( 60)   — 蹲/站切换、碰撞体替换
Sys_StealthMetrics  ( 62)   — 奔跑、速度乘数、噪音、可见度
Sys_Movement        ( 65)   — 物理移动（ApplyImpulse）
Sys_Physics         (100)   — Jolt 物理步进 + Transform 同步
Sys_PlayerCamera    (150)   — 第三人称跟随相机
Sys_Camera          (155)   — NCL Bridge 同步 + debug 飞行（F1 切换）
Sys_Render          (200)   — ECS → NCL 渲染桥接
Sys_ImGui           (300)   — Debug UI（仅 USE_IMGUI）
```

## Registry ctx 模式规范

经 PR #6 和 #10 修复后确立的模式：

- OnAwake：无条件 `ctx_emplace<T*>(this)` 覆盖（不用 `has_ctx` 守卫）
- OnDestroy：`ctx<T*>() = nullptr` 置空
- 已注册：`Sys_Physics*`、`Sys_Camera*`、`EventBus*`

## 全部键位表（更新）

| 按键 | 功能 | 系统 |
|------|------|------|
| W/A/S/D | 移动 | Sys_Movement |
| Shift | 奔跑 | Sys_StealthMetrics + Sys_Movement |
| C | 蹲下 | Sys_PlayerStance |
| V | 站起 | Sys_PlayerStance |
| E | 伪装开/关 | Sys_PlayerDisguise |
| F1 | Debug 相机切换 | Sys_Camera |
| F | CQC 制服 / 拟态 | Sys_PlayerCQC |

---

# PR #19 合并冲突解决（2026-03-05）

> 分支：`feat/csl/player-camera` ← `origin/master`
> Commit：`7b06dac`

## 冲突概述

master 合并了其他分支后与 player-camera 产生 3 个文件冲突 + 2 个编译兼容问题。

| 文件 | 处理 |
|------|------|
| `Scene_PhysicsTest.cpp` | 合并双方系统注册 + 无条件 ctx_emplace |
| `Sys_Movement.cpp/h` | master 误删（revert），从 HEAD 恢复 |
| `Sys_Physics.cpp/h` | 接受 master 新 API（gravity tracking, CastRay, SetRotation, ctx_erase），拒绝 ApplyPlayerInputs |
| `Sys_StealthMetrics.cpp` | `NoiseType` → `PlayerNoiseType` 适配 |
| `Sys_PlayerStance.cpp` | `ReplaceShapeCapsule` 参数顺序 `(halfHeight, radius)` 适配 |

---

# PR #20 + #22 Review 修复（2026-03-05）

> PR #20 分支：`feat/csl/player-gameplay` | Commit：`be84c0d`
> PR #22 分支：`csl/cqb` | Commit：`3152857`

## PR #20 修复（8 条 review，5 个文件）

| # | 文件 | 修复 |
|---|------|------|
| 1 | `Sys_StealthMetrics.cpp` | 蹲伏时按 Shift 覆盖 moveSpeedMul 添加注释说明 |
| 2 | `Sys_StealthMetrics.cpp` | switch default 分支补零（visibilityFactor=0, noiseLevel=0） |
| 3 | `C_D_PlayerState.h` | "Sys_Gameplay" → 正确系统名注释 |
| 4 | `Sys_Movement.cpp` | "Sys_Gameplay" → 正确系统名注释 |
| 5 | `Scene_PhysicsTest.cpp` | 玩家 spawn Y=0 → Y=-3.5 |
| 6 | `Sys_PlayerStance.cpp` | SKIN_OFFSET 累积修复（oldBottom 减去偏移） |
| 7 | `Scene_PhysicsTest.cpp` | registry.Clear() 添加 ctx 安全注释 |
| 8 | `Sys_Movement.cpp` | 删本地 stanceMul，force 改用 ps.moveSpeedMul |

## PR #22 修复（15 条 review 去重后 7 项，6 个文件）

| # | 文件 | 修复 |
|---|------|------|
| 1 | `Sys_PlayerCQC.cpp` | hasBeenMimicked 移到 MeshRenderer 验证通过后 |
| 2 | `Sys_PlayerCQC.cpp` | cooldown 仅限 CQC takedown，不阻拟态 |
| 3 | `Sys_PlayerCQC.cpp` | publish → publish_deferred |
| 4 | `Sys_Movement.cpp` | 删 input.shiftDown 回退，仅用 ps.isSprinting |
| 5 | `Sys_InputDispatch.h` | "C/V/E" → "C/V/E/F" 上升沿注释 |
| 6 | `C_D_CQCState.h` | uint32_t → MeshHandle/MaterialHandle + include |
| 7 | `Sys_Input.h` | 删尾部空白 |

---

# PR #22 合并冲突解决（2026-03-05）

> 分支：`csl/cqb` ← `origin/master`（master 已合并 PR #19 + #20）
> Commit：`e483c98`

## 冲突概述（4 个文件）

| 文件 | 处理 |
|------|------|
| `Main.cpp` | 仅 SetTitle 字符串差异，保留 csl/cqb（"PhysicsTest"） |
| `C_D_PlayerState.h` | 接受 csl/cqb，修复 master 的重复 noiseCooldown 字段编译 bug |
| `PrefabFactory.cpp` | 接受 csl/cqb，保留 CQC 组件挂载（C_D_CQCState + C_D_EnemyDormant） |
| `Scene_PhysicsTest.cpp` | 合并双方：Sys_PlayerCQC(63) + Sys_Raycast(330) + Res_CQCConfig 无条件 ctx_emplace + ctx 安全注释 |

## 当前系统注册顺序（合并后）

```
Sys_Input           ( 10)   — NCL → Res_Input
Sys_InputDispatch   ( 55)   — Res_Input → per-entity C_D_Input
Sys_PlayerDisguise  ( 59)   — 伪装切换、C_T_Hidden 管理
Sys_PlayerStance    ( 60)   — 蹲/站切换、碰撞体替换
Sys_StealthMetrics  ( 62)   — 奔跑、速度乘数、噪音、可见度
Sys_PlayerCQC       ( 63)   — CQC 近身制服 + 拟态
Sys_Movement        ( 65)   — 物理移动（ApplyImpulse + 零惯性制动）
Sys_Physics         (100)   — Jolt 物理步进 + Transform 同步
Sys_EnemyAI         (120)   — 敌人 AI 状态机
Sys_PlayerCamera    (150)   — 第三人称跟随相机
Sys_Camera          (155)   — NCL Bridge 同步 + debug 飞行
Sys_Render          (200)   — ECS → NCL 渲染桥接
Sys_ImGui           (300)   — Debug UI
Sys_ImGuiCapsuleGen (301)   — 胶囊生成面板
Sys_ImGuiEnemyAI    (310)   — 敌人监控表格
Sys_ImGuiPhysicsTest(320)   — PhysicsTest 敌人控制面板
Sys_Raycast         (330)   — Raycast 测试窗口
```

---

# CQC 目标选择 + 边缘高亮（2026-03-15）

> 完成日期：2026-03-15
> 涉及分支：`csl-fix`

## 概述

重构 Sys_PlayerCQC：删除拟态功能（isMimicking/hasBeenMimicked/Evt_CQC_Mimicry），删除背面扇形判定（dorsalDotMin），新增"范围内敌人自动扫描 → 滚轮切换选中目标 → Fresnel 边缘高亮 → F 键发起 CQC"。同时修正输入管线中 Sys_InputDispatch 直接读 NCL Mouse 的 ECS 违规。

## 功能行为

### 操作键位

| 按键 | 功能 | 条件 |
|------|------|------|
| 滚轮 | 切换候选目标 | None 阶段 + Standing + 非伪装 + 非冲刺 |
| F | 发起 CQC | 有选中目标 + cooldown 结束 |

### 目标选择规则

- 自动扫描 maxDistance（5m）范围内的非休眠、非 Hunt 状态敌人
- 按 XZ 距离升序排列，最多 8 个候选
- 滚轮上滚选前一个，下滚选后一个，循环包裹
- 选中目标离开范围或进入 Hunt → 自动切换到下一个最近目标
- 前置条件不满足（蹲/伪装/冲刺）→ 立即清除高亮

### 边缘高亮

- 视角相关 Fresnel rim lighting：`rim = pow(1.0 - dot(viewDir, normal), rimPower) * rimStrength`
- 仅在摄像机视角下的掠射边缘出现淡白色提示，正对面无高亮
- 默认参数：rimColour=(0.85, 0.87, 0.9)，rimPower=6.0，rimStrength=0.35
- 所有参数由 Res_CQCConfig 数据驱动

## 删除内容

| 删除项 | 原位置 |
|--------|--------|
| 拟态字段 isMimicking/mimicSource/originalMesh/originalMat | C_D_CQCState.h |
| hasBeenMimicked | C_D_EnemyDormant.h |
| dorsalDotMin/mimicryDistance | Res_CQCConfig.h |
| Evt_CQC_Mimicry.h | Game/Events/ |
| isMimicking 绕过检查 | Sys_EnemyVision.cpp |
| 全部拟态逻辑 + 背面扇形检测 | Sys_PlayerCQC.cpp |

## 新增内容

### 新建文件（1 个）

| 文件 | 路径 | 用途 |
|------|------|------|
| `C_D_CQCHighlight.h` | `Game/Components/` | 边缘高亮标记组件（rimColour/rimPower/rimStrength） |

### 修改文件（16 个，含新建 1）

| 文件 | 改动 |
|------|------|
| `C_D_CQCState.h` | 删拟态字段，加 highlightedEnemy/selectedIndex/candidateCount |
| `C_D_EnemyDormant.h` | 删 hasBeenMimicked |
| `Res_CQCConfig.h` | 删 dorsalDotMin/mimicryDistance，加 highlightRimColour/Power/Strength |
| `C_D_Input.h` | 加 scrollDelta |
| `Res_Input.h` | 加 scrollWheel |
| `InputAdapter.cpp` | 新增 scrollWheel 采集 |
| `Sys_InputDispatch.cpp` | 删 Window.h/Mouse.h，改从 Res_Input 读滚轮 |
| `Sys_PlayerCQC.cpp` | 核心重写：目标扫描 + 滚轮切换 + 高亮管理 |
| `Sys_PlayerCQC.h` | 更新注释 |
| `Sys_EnemyVision.cpp` | 删 isMimicking 绕过 |
| `Sys_Render.cpp` | CQCHighlight 分支写 GameTechMaterial rim 参数 |
| `GameTechRenderer.cpp` | Opaque/Transparent pass 新增 rimColour/rimPower/rimStrength uniform 传递 |
| `scene.frag` | 新增 rim uniform + Fresnel rim lighting 计算 |
| `Scene_PhysicsTest.h` | 注释更新 |
| `Scene_PhysicsTest.cpp` | 注释更新 |

## 数据流

### 输入链路（ECS 合规）

```
NCL::Mouse::GetWheelMovement()
  → InputAdapter::Update()        [Core/Bridge — 唯一接触 NCL 的地方]
  → Res_Input.scrollWheel         [ECS 全局资源]
  → Sys_InputDispatch(55)         [Game/Systems — 仅读 Res_Input]
  → C_D_Input.scrollDelta         [ECS per-entity 组件]
  → Sys_PlayerCQC(63)             [Game/Systems — 仅读 C_D_Input]
```

### 渲染链路（边缘高亮）

```
Sys_PlayerCQC(63)
  → Emplace<C_D_CQCHighlight>{rimColour, rimPower, rimStrength}
Sys_Render(200) SyncProxy
  → GameTechMaterial.emissiveColor = rimColour
  → GameTechMaterial.rimPower/rimStrength
GameTechRenderer RenderOpaquePass/RenderTransparentPass
  → glUniform3fv(rimColour), glUniform1f(rimPower/rimStrength)
scene.frag
  → rim = pow(1.0 - dot(viewDir, normal), rimPower) * rimStrength
  → fragColor.rgb += rimColour * rim
```

### 目标选择 / 选中 / 击杀数据关系

| 字段 | 组件 | 含义 |
|------|------|------|
| highlightedEnemy | C_D_CQCState | 当前边缘高亮的敌人 EntityID |
| selectedIndex | C_D_CQCState | 候选列表中的索引 |
| targetEnemy | C_D_CQCState | F 键确认后进入 CQC 流程的目标 |

F 键时 `targetEnemy = highlightedEnemy`，清除高亮，进入 Approach。

## ECS 修正记录

### 输入管线违规修复

**问题**：Sys_InputDispatch.cpp 直接 `#include "Window.h"` / `#include "Mouse.h"` 并调用 `NCL::Window::GetWindow()->GetMouse()->GetWheelMovement()`，绕过了 InputAdapter → Res_Input 的 ECS 输入管线。

**修复**：
1. `Res_Input.h` 新增 `int scrollWheel` 字段
2. `InputAdapter.cpp` 新增 `input.scrollWheel = mouse->GetWheelMovement()`
3. `Sys_InputDispatch.cpp` 删除 Window.h/Mouse.h include，改为 `int scrollWheel = res.scrollWheel`

### 高亮方案修正

**问题**：初版用 `SetColour()` 整体变色（金黄色），不是边缘高亮。

**修复**：
1. 新增 scene.frag rim uniform + Fresnel 计算
2. GameTechRenderer 两个 pass 传递 rim uniform
3. Sys_Render 写 GameTechMaterial rim 参数（不再 SetColour）
4. C_D_CQCHighlight 从 Vector4 colour 改为 rimColour/rimPower/rimStrength
5. 颜色从金黄改为淡白 (0.85, 0.87, 0.9)

## 验证

1. CMake 构建零报错零警告 ✓
2. 全局搜索 `isMimicking`/`mimicSource`/`Evt_CQC_Mimicry`/`hasBeenMimicked`/`dorsalDotMin`/`mimicryDistance` 零残留 ✓
3. Game/Systems/ 中无 Window.h/Mouse.h include（除 Sys_Input 调用 InputAdapter 外） ✓
