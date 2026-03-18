/**
 * @file Res_InputConfig.h
 * @brief 玩家动作键 + 对话键配置资源（数据驱动，注册到 registry ctx）。
 *
 * 消除 Sys_InputDispatch / Sys_Chat / Sys_Network 中的 KeyCodes 硬编码，
 * 运行时可通过修改此资源实现键位重绑定。
 */
#pragma once

#include "Keyboard.h"

namespace ECS {

/// @brief 玩家动作键 + 对话方向键配置（数据驱动，注册到 registry ctx）
struct Res_InputConfig {
    // ── 玩家动作键 ──
    NCL::KeyCodes::Type keySprint     = NCL::KeyCodes::SHIFT;
    NCL::KeyCodes::Type keyCrouch     = NCL::KeyCodes::C;
    NCL::KeyCodes::Type keyStand      = NCL::KeyCodes::V;
    NCL::KeyCodes::Type keyWeaponUse  = NCL::KeyCodes::E;
    NCL::KeyCodes::Type keyCQC        = NCL::KeyCodes::F;
    NCL::KeyCodes::Type keyGadgetUse  = NCL::KeyCodes::Q;
    NCL::KeyCodes::Type keyDisguise   = NCL::KeyCodes::G;

    // ── 道具使用键（数字键 1-5）──
    NCL::KeyCodes::Type keyItem1      = NCL::KeyCodes::NUM1;
    NCL::KeyCodes::Type keyItem2      = NCL::KeyCodes::NUM2;
    NCL::KeyCodes::Type keyItem3      = NCL::KeyCodes::NUM3;
    NCL::KeyCodes::Type keyItem4      = NCL::KeyCodes::NUM4;
    NCL::KeyCodes::Type keyItem5      = NCL::KeyCodes::NUM5;

    // ── 对话方向键（Sys_Chat + Sys_Network 共用）──
    NCL::KeyCodes::Type keyChatUp     = NCL::KeyCodes::UP;
    NCL::KeyCodes::Type keyChatDown   = NCL::KeyCodes::DOWN;
    NCL::KeyCodes::Type keyChatLeft   = NCL::KeyCodes::LEFT;
    NCL::KeyCodes::Type keyChatRight  = NCL::KeyCodes::RIGHT;
};

} // namespace ECS
