/**
 * @file C_D_DDoSFrozen.h
 * @brief DDoS 道具冻结状态组件（道具 003）。
 *
 * @details
 * 挂载在被 DDoS 道具命中的目标实体上。
 * Sys_ItemEffects 每帧递减 frozenTimer，归零时移除此组件并恢复目标移动能力。
 *
 * ## 效果描述
 * 目标被冻结泡沫枪击中后，暂停移动 5 秒（frozenDuration = 5.0f）。
 *
 * @see Sys_ItemEffects.h
 */
#pragma once

#include "Core/ECS/EntityID.h"

namespace ECS {

/**
 * @brief DDoS 冻结状态数据组件
 *
 * 挂载在被冻结的实体上。frozenTimer 归零后由 Sys_ItemEffects 移除此组件。
 * cageEntity 记录关联的囚笼 VFX 实体，解冻时一并销毁。
 */
struct C_D_DDoSFrozen {
    static constexpr float kFrozenDuration = 5.0f; ///< 冻结持续时间（秒）

    float    frozenTimer = kFrozenDuration; ///< 剩余冻结时间（秒），倒计时
    EntityID cageEntity  = Entity::NULL_ENTITY; ///< 关联的囚笼 VFX 实体（防重复生成 + 到期销毁）
};

} // namespace ECS
