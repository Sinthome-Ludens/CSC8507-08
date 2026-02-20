/**
 * @file C_D_DebugName.h
 * @brief 调试名称组件：存储实体的可读字符串标识
 *
 * @details
 * `C_D_DebugName` 为每个实体提供一个人类可读的名称，用于日志输出、性能分析和编辑器检视。
 *
 * ## 命名规范
 *
 * 根据游戏开发.md 第 172-181 行，实体调试名称必须使用全大写前缀：
 *
 * **格式**：`ENTITY_[主类型]_[描述1]_[描述2]_[描述3]`
 *
 * **示例**：
 * - `ENTITY_Player_Main` - 主玩家
 * - `ENTITY_Enemy_Orc_01` - 兽人敌人 1 号
 * - `ENTITY_Item_Key_Gold` - 金钥匙道具
 * - `ENTITY_Trap_Spike_North` - 北侧尖刺陷阱
 *
 * ## 设计约束
 *
 * 1. **固定长度数组**：`char name[64]`，避免动态内存分配（`std::string` 破坏 POD 性质）。
 * 2. **空字符结尾**：C 风格字符串，兼容 `printf` / `std::cout` 输出。
 * 3. **大小限制**：64 字节（包含 '\0'），超长名称会被截断。
 * 4. **必选组件**：所有实体在 `PrefabFactory` 创建时**必须**挂载此组件。
 *
 * ## 使用场景
 *
 * - **日志输出**：`LOG_INFO("ENTITY_Player_Main health low")`
 * - **性能分析**：标记 CPU/GPU Profiler 时间线上的实体操作
 * - **碰撞调试**：`Debug::DrawLine` 时显示实体名称
 * - **编辑器检视**：在 ImGui 窗口中显示实体列表
 *
 * ## 性能考量
 *
 * - **内存占用**：64 字节 / 实体
 * - **查询开销**：O(1) 直接内存访问（无哈希表查找）
 * - **Release 优化**：可选择性禁用此组件以节省内存（需修改 PrefabFactory）
 *
 * ## 赋值示例
 *
 * @code
 * // 在 PrefabFactory 中
 * auto player = registry.Create();
 * auto& name = registry.Emplace<C_D_DebugName>(player);
 * std::strncpy(name.name, "ENTITY_Player_Main", sizeof(name.name) - 1);
 * name.name[sizeof(name.name) - 1] = '\0'; // 确保空字符结尾
 * @endcode
 *
 * @note 使用 `std::strncpy` 或 `snprintf` 防止缓冲区溢出。
 *
 * @see PrefabFactory (创建实体时强制挂载此组件)
 */

#pragma once

#include <cstring> // for std::strncpy

namespace ECS {

/**
 * @brief 调试名称组件：实体的人类可读标识符（固定长度字符数组）。
 *
 * @details
 * 所有实体在创建时**必须**挂载此组件，名称格式遵循全大写 `ENTITY_` 前缀约定。
 */
struct C_D_DebugName {
    char name[64] = ""; ///< 实体调试名称（C 风格字符串，最多 63 字符 + '\0'）
};

} // namespace ECS
