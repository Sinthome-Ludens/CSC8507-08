/**
 * @file C_D_Interactable.h
 * @brief 可交互标记组件：标识实体可被玩家交互，并存储交互参数
 *
 * @details
 * 当实体挂载此组件且 enabled == true 时，UI 系统会在玩家靠近时显示浮动交互提示
 * （如 "[E] PICK UP"）。检测距离、提示偏移、交互类型均可按实体独立配置。
 *
 * ## InteractionType 枚举
 *
 * | 值 | 含义 | 默认提示文本 |
 * |----|------|-------------|
 * | PickUp | 拾取物品 | [E] PICK UP |
 * | Use | 使用/交互 | [E] USE |
 * | Hack | 黑入终端 | [E] HACK |
 * | Eliminate | 消除目标 | [E] ELIMINATE |
 * | Examine | 检查/阅读 | [E] EXAMINE |
 *
 * ## 设计约束
 *
 * 1. POD 结构体（≤64 字节），无动态内存分配
 * 2. label[20] 为自定义文本；空串时由 UI 渲染模块按 type 自动生成默认文本
 * 3. enabled 可在运行时动态开关（如物品拾取后设 false）
 *
 * @see UI_Interaction.h (渲染交互提示)
 * @see PrefabFactory::CreateInteractable (创建可交互实体)
 */

#pragma once
#include <cstdint>

namespace ECS {

enum class InteractionType : uint8_t {
    PickUp    = 0,   // 拾取物品
    Use       = 1,   // 使用/交互
    Hack      = 2,   // 黑入终端
    Eliminate = 3,   // 消除目标
    Examine   = 4,   // 检查/阅读
};

struct C_D_Interactable {
    InteractionType type      = InteractionType::PickUp;
    uint8_t         padding_  = 0;
    bool            enabled   = true;    // false 时不显示提示
    uint8_t         reserved_ = 0;
    float           radius    = 3.0f;    // 检测半径（米）
    float           offsetY   = 1.5f;    // 提示显示在实体上方的偏移（米）
    char            label[20] = {};      // 自定义标签，空串则按 type 生成默认
};
// sizeof = 1+1+1+1+4+4+20 = 32 字节

} // namespace ECS
