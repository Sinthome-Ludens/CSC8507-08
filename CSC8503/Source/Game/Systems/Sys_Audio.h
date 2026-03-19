/**
 * @file Sys_Audio.h
 * @brief FMOD 音频系统 — BGM 1:1 状态机 + SFX one-shot 播放。
 *
 * 唯一接触 FMOD 的系统。其他系统只写 Res_AudioState 或发 Evt_Audio_PlaySFX。
 * 优先级 275（在 Door(270) 之后、ImGui(300) 之前）。
 */
#pragma once

#include "Core/ECS/BaseSystem.h"
#include "Game/Components/Res_AudioConfig.h"
#include <cstdint>
#include <vector>

namespace FMOD { class System; class Sound; class Channel; class ChannelGroup; }

namespace ECS {

class Sys_Audio : public ISystem {
public:
    void OnAwake  (Registry& registry)           override;
    void OnUpdate (Registry& registry, float dt) override;
    void OnDestroy(Registry& registry)           override;

private:
    void LoadSound(const char* path, bool loop, int index, bool isBgm);
    void StopBgm();
    void PlayBgm(BgmId id);
    void PlaySfx(SfxId id);

    bool m_Initialized = false;

    FMOD::System*       m_FmodSystem = nullptr;
    FMOD::ChannelGroup* m_BgmGroup  = nullptr;
    FMOD::ChannelGroup* m_SfxGroup  = nullptr;
    FMOD::Channel*      m_BgmChannel = nullptr;

    FMOD::Sound* m_BgmSounds[(int)BgmId::COUNT] = {};
    FMOD::Sound* m_SfxSounds[(int)SfxId::COUNT] = {};

    std::vector<SfxId> m_PendingSfx;

    uint64_t m_DeathSubId = 0;
    uint64_t m_AlertSubId = 0;
    uint64_t m_SfxSubId   = 0;
};

} // namespace ECS
