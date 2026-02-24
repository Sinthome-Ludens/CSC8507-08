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
