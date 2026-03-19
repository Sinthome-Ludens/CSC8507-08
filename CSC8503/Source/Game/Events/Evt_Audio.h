/**
 * @file Evt_Audio.h
 * @brief SFX 播放请求事件（延迟分发，Sys_Audio 订阅消费）。
 *
 * 业务系统通过 bus->publish_deferred<Evt_Audio_PlaySFX>({SfxId::UIClick}) 触发音效。
 * BGM 切换不用事件 — 直接写 Res_AudioState.requestedBgm。
 */
#pragma once

#include "Game/Components/Res_AudioConfig.h"

namespace ECS {

struct Evt_Audio_PlaySFX {
    SfxId id = SfxId::None;
};

} // namespace ECS
