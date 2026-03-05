#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 输入分发系统（优先级 55）
 *
 * 职责：从全局 Res_Input 读取输入状态，转换为每个玩家实体的 C_D_Input。
 * 包含 C/V/E/F 上升沿检测。
 *
 * 读：Res_Input（Registry ctx）
 * 写：C_D_Input（per-entity）
 */
class Sys_InputDispatch : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

private:
    // ── 上升沿检测状态 ──
    bool m_CWasPressed = false;
    bool m_VWasPressed = false;
    bool m_EWasPressed = false;
    bool m_FWasPressed = false;
};

} // namespace ECS
