#pragma once

namespace ECS {

/**
 * @brief 主相机标签（标签组件）
 *
 * 标记场景中唯一的主渲染相机实体。
 * Sys_Camera 通过 view<C_T_MainCamera>() 定位该实体，
 * 不含任何成员变量（零内存开销）。
 *
 * @note 场景中应有且仅有一个实体挂载此标签。
 */
struct C_T_MainCamera {};

} // namespace ECS
