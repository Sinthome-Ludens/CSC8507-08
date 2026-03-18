#pragma once

#include "Core/ECS/BaseSystem.h"

namespace ECS {

/**
 * @brief 输入分发系统（优先级 55）
 *
 * 职责：从全局 Res_Input 读取输入状态，转换为每个玩家实体的 C_D_Input。
 * 包含 C/V/E/F 上升沿检测，以及道具使用键 1-5 上升沿检测。
 *
 * 读：Res_Input（Registry ctx）
 * 写：C_D_Input（per-entity）
 */
class Sys_InputDispatch : public ISystem {
public:
    void OnUpdate(Registry& registry, float dt) override;

private:
    // ── 动作键上升沿检测状态 ──
    bool m_CWasPressed = false;
    bool m_VWasPressed = false;
    bool m_EWasPressed = false;
    bool m_FWasPressed = false;
    bool m_QWasPressed = false;
    bool m_GWasPressed = false;

    // ── 道具使用键 1-5 上升沿检测状态 ──
    bool m_Key1WasPressed = false; ///< 数字键 1（全息诱饵炸弹）
    bool m_Key2WasPressed = false; ///< 数字键 2（光子雷达）
    bool m_Key3WasPressed = false; ///< 数字键 3（DDoS）
    bool m_Key4WasPressed = false; ///< 数字键 4（流窜 AI）
    bool m_Key5WasPressed = false; ///< 数字键 5（靶向打击）
};

} // namespace ECS
