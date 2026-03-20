/**
 * @file Sys_InputDispatch.cpp
 * @brief 输入分发系统实现：Res_Input → per-entity C_D_Input。
 */
#include "Sys_InputDispatch.h"
#include "Game/Utils/PauseGuard.h"

#include "Keyboard.h"

#include "Game/Components/Res_Input.h"
#include "Game/Components/C_D_Input.h"
#include "Game/Components/C_T_Player.h"
#include "Game/Systems/Sys_Camera.h"
#include "Game/Components/Res_InputConfig.h"

#include <cmath>

namespace ECS {

void Sys_InputDispatch::OnUpdate(Registry& registry, float /*dt*/) {
    PAUSE_GUARD(registry);
    if (!registry.has_ctx<Res_Input>()) return;
    auto& res = registry.ctx<Res_Input>();

    Res_InputConfig defaultCfg;
    const auto& cfg = registry.has_ctx<Res_InputConfig>() ? registry.ctx<Res_InputConfig>() : defaultCfg;

    // ── 从 Res_Input 读取滚轮输入（由 InputAdapter 采集） ──
    int scrollWheel = res.scrollWheel;

    // ── 从 Res_Input 读取移动输入 ──
    float inputX = res.axisX;          // A=-1, D=+1 → 同向
    float inputZ = -res.axisY;         // W=+1 → 游戏中 W 是 -Z 方向，取反

    float inputLen = std::sqrt(inputX * inputX + inputZ * inputZ);
    if (inputLen > 1.0f) {
        inputX /= inputLen;
        inputZ /= inputLen;
    }
    bool hasInput = (inputLen > cfg.inputDeadzone);
    bool shiftDown = res.keyStates[cfg.keySprint];

    // ── C/V/E/F/Q/G 上升沿检测 ──
    bool cDown = res.keyStates[cfg.keyCrouch];
    bool cPressed = cDown && !m_CWasPressed;
    m_CWasPressed = cDown;

    bool vDown = res.keyStates[cfg.keyStand];
    bool vPressed = vDown && !m_VWasPressed;
    m_VWasPressed = vDown;

    bool eDown = res.keyStates[cfg.keyWeaponUse];
    bool ePressed = eDown && !m_EWasPressed;
    m_EWasPressed = eDown;

    bool fDown = res.keyStates[cfg.keyCQC];
    bool fPressed = fDown && !m_FWasPressed;
    m_FWasPressed = fDown;

    bool qDown = res.keyStates[cfg.keyGadgetUse];
    bool qPressed = qDown && !m_QWasPressed;
    m_QWasPressed = qDown;

    bool gDown = res.keyStates[cfg.keyDisguise];
    bool gPressed = gDown && !m_GWasPressed;
    m_GWasPressed = gDown;

    // ── Debug 模式下阻断移动输入（Sync 关闭时玩家不动）──
    bool blockMovement = false;
    if (registry.has_ctx<Sys_Camera*>()) {
        auto* camSys = registry.ctx<Sys_Camera*>();
        if (camSys && camSys->IsDebugMode() && !camSys->IsSyncToPlayer()) {
            blockMovement = true;
        }
    }

    if (blockMovement) {
        inputX = 0.0f;
        inputZ = 0.0f;
        hasInput = false;
        shiftDown = false;
        scrollWheel = 0;
    }

    // ── 写入所有玩家实体的 C_D_Input ──
    registry.view<C_T_Player, C_D_Input>().each(
        [&](EntityID /*id*/, C_T_Player&, C_D_Input& input) {
            input.moveX              = inputX;
            input.moveZ              = inputZ;
            input.hasInput           = hasInput;
            input.shiftDown          = shiftDown;
            input.crouchJustPressed   = cPressed;
            input.standJustPressed    = vPressed;
            input.disguiseJustPressed = gPressed;   // G = disguise (was E)
            input.cqcJustPressed      = fPressed;
            input.gadgetUseJustPressed = qPressed;  // Q = use active gadget
            input.weaponUseJustPressed = ePressed;  // E = use active weapon
            input.scrollDelta        = scrollWheel;
        }
    );
}

} // namespace ECS
