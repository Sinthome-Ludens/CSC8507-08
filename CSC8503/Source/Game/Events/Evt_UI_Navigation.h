/**
 * @file Evt_UI_Navigation.h
 * @brief UI 导航事件：菜单确认和画面切换
 */
#pragma once

#include <cstdint>
#include "Game/Components/Res_UIState.h"

namespace ECS {

struct Evt_UI_MenuConfirm {
    UIScreen fromScreen;
    uint8_t  selectedIndex;
};

struct Evt_UI_ScreenChange {
    UIScreen from;
    UIScreen to;
};

} // namespace ECS
