/**
 * @file Res_UIKeyConfig.h
 * @brief UI / 菜单 / Debug / 相机控制键配置资源（数据驱动，注册到 registry ctx）。
 *
 * 消除 Sys_UI / Sys_Camera / UI_Menus / UI_GameOver 等文件中的 KeyCodes 硬编码，
 * 运行时可通过修改此资源实现键位重绑定。
 */
#pragma once

#include "Keyboard.h"
#include "Mouse.h"

namespace ECS {

/// @brief UI / 菜单 / Debug / 相机键配置（数据驱动，注册到 registry ctx）
struct Res_UIKeyConfig {
    // ── 菜单导航（主绑定 + Alt 绑定）──
    NCL::KeyCodes::Type keyMenuUp       = NCL::KeyCodes::W;
    NCL::KeyCodes::Type keyMenuDown     = NCL::KeyCodes::S;
    NCL::KeyCodes::Type keyMenuLeft     = NCL::KeyCodes::A;
    NCL::KeyCodes::Type keyMenuRight    = NCL::KeyCodes::D;
    NCL::KeyCodes::Type keyMenuUpAlt    = NCL::KeyCodes::UP;
    NCL::KeyCodes::Type keyMenuDownAlt  = NCL::KeyCodes::DOWN;
    NCL::KeyCodes::Type keyMenuLeftAlt  = NCL::KeyCodes::LEFT;
    NCL::KeyCodes::Type keyMenuRightAlt = NCL::KeyCodes::RIGHT;

    // ── 确认 / 取消 ──
    NCL::KeyCodes::Type keyConfirm      = NCL::KeyCodes::RETURN;
    NCL::KeyCodes::Type keyConfirmAlt   = NCL::KeyCodes::SPACE;
    NCL::MouseButtons::Type mouseConfirm = NCL::MouseButtons::Left;

    // ── UI 功能键 ──
    NCL::KeyCodes::Type keyMenuBack     = NCL::KeyCodes::ESCAPE;
    NCL::KeyCodes::Type keyInventory    = NCL::KeyCodes::I;
    NCL::KeyCodes::Type keyItemWheel    = NCL::KeyCodes::TAB;
    NCL::KeyCodes::Type keyCursorFree   = NCL::KeyCodes::MENU;

    // ── Debug 键 ──
    NCL::KeyCodes::Type keyDevMode           = NCL::KeyCodes::F1;
    NCL::KeyCodes::Type keyDebugAlertCycle   = NCL::KeyCodes::F2;
    NCL::KeyCodes::Type keyDebugCountdown    = NCL::KeyCodes::F3;
    NCL::KeyCodes::Type keyDebugGameOver     = NCL::KeyCodes::F5;
    NCL::KeyCodes::Type keyDebugNoiseCycle   = NCL::KeyCodes::F6;
    NCL::KeyCodes::Type keyDebugCRT          = NCL::KeyCodes::F7;
    NCL::KeyCodes::Type keyDebugToast        = NCL::KeyCodes::F8;
    NCL::KeyCodes::Type keyDebugInteractables = NCL::KeyCodes::F9;

    // ── Debug 相机移动键 ──
    NCL::KeyCodes::Type keyCamForward   = NCL::KeyCodes::W;
    NCL::KeyCodes::Type keyCamBackward  = NCL::KeyCodes::S;
    NCL::KeyCodes::Type keyCamLeft      = NCL::KeyCodes::A;
    NCL::KeyCodes::Type keyCamRight     = NCL::KeyCodes::D;
    NCL::KeyCodes::Type keyCamDown      = NCL::KeyCodes::Q;
    NCL::KeyCodes::Type keyCamUp        = NCL::KeyCodes::E;

    // ── Deploy 快捷键（MissionSelect 专用）──
    NCL::KeyCodes::Type keyDeploy       = NCL::KeyCodes::C;
};

} // namespace ECS
