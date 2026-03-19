/**
 * @file Sys_Audio.cpp
 * @brief FMOD 音频系统实现：初始化、BGM 1:1 状态机、SFX one-shot、警戒度自动驱动。
 *
 * 算法概要：
 *  - OnAwake: 创建 FMOD System，预加载全部 BGM/SFX，订阅 Evt_Death / Evt_EnemyAlertChange / Evt_Audio_PlaySFX
 *  - OnUpdate:
 *    1. 警戒度自动驱动：读 Res_GameState.alertLevel 映射到 Normal/Tense/Danger
 *    2. BGM 状态机：requestedBgm != currentBgm → 停旧播新（循环）
 *    3. SFX 队列消费：逐个 playSound one-shot
 *    4. 音量同步：Res_UIState.masterVolume / sfxVolume → FMOD ChannelGroup
 *    5. FMOD::System::update()
 *  - OnDestroy: 停止所有播放，释放 Sound，关闭 FMOD System
 */
#include "Sys_Audio.h"

#include <fmod.hpp>
#include <fmod_errors.h>
#include <cstdlib>
#include <random>
#include <string>

#include "Assets.h"
#include "Game/Components/Res_AudioConfig.h"
#include "Game/Components/Res_GameState.h"
#include "Game/Components/Res_UIState.h"
#include "Game/Events/Evt_Audio.h"
#include "Game/Events/Evt_Death.h"
#include "Game/Events/Evt_EnemyAlertChange.h"
#include "Core/ECS/EventBus.h"
#include "Game/Utils/Log.h"

namespace ECS {

// ============================================================
// OnAwake
// ============================================================
void Sys_Audio::OnAwake(Registry& registry) {
    m_Rng.seed(std::random_device{}());

    FMOD_RESULT result = FMOD::System_Create(&m_FmodSystem);
    if (result != FMOD_OK) {
        LOG_ERROR("[Sys_Audio] FMOD System_Create failed: " << FMOD_ErrorString(result));
        m_Initialized = false;
        return;
    }

    if (!registry.has_ctx<Res_AudioConfig>()) {
        registry.ctx_emplace<Res_AudioConfig>();
    }
    const auto& initCfg = registry.ctx<Res_AudioConfig>();
    result = m_FmodSystem->init(initCfg.maxChannels, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) {
        LOG_ERROR("[Sys_Audio] FMOD init failed: " << FMOD_ErrorString(result));
        m_FmodSystem->release();
        m_FmodSystem = nullptr;
        m_Initialized = false;
        return;
    }

    m_FmodSystem->createChannelGroup("BGM", &m_BgmGroup);
    m_FmodSystem->createChannelGroup("SFX", &m_SfxGroup);

    // ── 注册 ctx 资源（Res_AudioConfig 已在 init 前 emplace）──
    if (!registry.has_ctx<Res_AudioState>()) {
        registry.ctx_emplace<Res_AudioState>();
    }

    // 场景切换后 FMOD System 重建，旧 channel 已失效
    // 强制 reset currentBgm 让状态机重新触发播放
    {
        auto& audioState = registry.ctx<Res_AudioState>();
        audioState.currentBgm = BgmId::None;
    }

    const auto& cfg = registry.ctx<Res_AudioConfig>();

    // ── 预加载所有 BGM（loop 模式）──
    for (int i = 1; i < (int)BgmId::COUNT; ++i) {
        LoadSound(cfg.bgmPaths[i], true, i, true);
    }

    // ── 预加载所有 SFX（one-shot 模式）──
    for (int i = 1; i < (int)SfxId::COUNT; ++i) {
        LoadSound(cfg.sfxPaths[i], false, i, false);
    }

    // ── 订阅事件 ──
    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            /**
             * 敌人死亡 → 50:50 随机播放 EnemyKillA 或 EnemyKillB
             * 玩家死亡 → 播放 PlayerDeath
             * 直接调用 PlaySfx（flush 时立即播放，不延迟到下一帧）
             */
            m_DeathSubId = bus->subscribe<Evt_Death>(
                [this](const Evt_Death& e) {
                    if (!m_Initialized) return;
                    if (e.deathType == DeathType::EnemyHpZero) {
                        static const SfxId kEnemyKillVariants[] = { SfxId::EnemyKillA, SfxId::EnemyKillB, SfxId::EnemyKillC };
                        PlaySfx(kEnemyKillVariants[m_Rng() % 3]);
                    }
                    if (e.deathType == DeathType::PlayerCaptured
                        || e.deathType == DeathType::PlayerTriggerDie) {
                        PlaySfx(SfxId::PlayerDeath);
                    }
                }
            );

            /**
             * 敌人进入 Hunt → 播放 HuntEnter SFX
             */
            m_AlertSubId = bus->subscribe<Evt_EnemyAlertChange>(
                [this](const Evt_EnemyAlertChange& e) {
                    if (!m_Initialized) return;
                    if (e.newState == EnemyState::Hunt) {
                        PlaySfx(SfxId::HuntEnter);
                    }
                }
            );

            /**
             * 通用 SFX 请求事件（业务系统主动发）
             * 直接调用 PlaySfx，flush 时立即播放
             */
            m_SfxSubId = bus->subscribe<Evt_Audio_PlaySFX>(
                [this](const Evt_Audio_PlaySFX& e) {
                    if (!m_Initialized) return;
                    if (e.id != SfxId::None) {
                        PlaySfx(e.id);
                    }
                }
            );
        }
    }

    m_Initialized = true;
    LOG_INFO("[Sys_Audio] OnAwake - FMOD initialized, "
             << (int)BgmId::COUNT - 1 << " BGM + "
             << (int)SfxId::COUNT - 1 << " SFX loaded.");
}

// ============================================================
// OnUpdate
// ============================================================
void Sys_Audio::OnUpdate(Registry& registry, float /*dt*/) {
    if (!m_Initialized) return;
    if (!registry.has_ctx<Res_AudioState>()) return;

    auto& state = registry.ctx<Res_AudioState>();
    const auto& cfg = registry.ctx<Res_AudioConfig>();

    // ── 1. 警戒度自动驱动游玩 BGM（仅在 isGameplay 且非 bgmOverride 时）──
    if (state.isGameplay && !state.bgmOverride
        && registry.has_ctx<Res_GameState>()) {
        const auto& gs = registry.ctx<Res_GameState>();
        if (!gs.isGameOver) {
            BgmId target = BgmId::GameplayNormal;
            if (gs.alertLevel > cfg.alertThresholdTense) {
                target = BgmId::GameplayDanger;
            } else if (gs.alertLevel > cfg.alertThresholdNormal) {
                target = BgmId::GameplayTense;
            }
            if (target != state.currentBgm && state.requestedBgm == BgmId::None) {
                state.requestedBgm = target;
            }
        }
    }

    // ── 2. BGM 状态机（1:1，不可重叠，循环播放）──
    if (state.requestedBgm != BgmId::None
        && state.requestedBgm != state.currentBgm) {
        StopBgm();
        if (PlayBgm(state.requestedBgm)) {
            state.currentBgm = state.requestedBgm;
        }
        state.requestedBgm = BgmId::None;
    }

    // ── 3. SFX：事件回调中已直接调用 PlaySfx，无需队列消费 ──

    // ── 4. 音量同步 ──
    if (registry.has_ctx<Res_UIState>()) {
        const auto& ui = registry.ctx<Res_UIState>();
        if (m_BgmGroup) m_BgmGroup->setVolume(ui.masterVolume * ui.bgmVolume);
        if (m_SfxGroup) m_SfxGroup->setVolume(ui.masterVolume * ui.sfxVolume);
    }

    // ── 5. FMOD 帧更新（必须每帧调用）──
    m_FmodSystem->update();
}

// ============================================================
// OnDestroy
// ============================================================
void Sys_Audio::OnDestroy(Registry& registry) {
    if (registry.has_ctx<EventBus*>()) {
        auto* bus = registry.ctx<EventBus*>();
        if (bus) {
            if (m_DeathSubId) bus->unsubscribe<Evt_Death>(m_DeathSubId);
            if (m_AlertSubId) bus->unsubscribe<Evt_EnemyAlertChange>(m_AlertSubId);
            if (m_SfxSubId)   bus->unsubscribe<Evt_Audio_PlaySFX>(m_SfxSubId);
        }
    }
    m_DeathSubId = 0;
    m_AlertSubId = 0;
    m_SfxSubId   = 0;

    if (m_FmodSystem) {
        StopBgm();
        for (int i = 0; i < (int)BgmId::COUNT; ++i) {
            if (m_BgmSounds[i]) { m_BgmSounds[i]->release(); m_BgmSounds[i] = nullptr; }
        }
        for (int i = 0; i < (int)SfxId::COUNT; ++i) {
            if (m_SfxSounds[i]) { m_SfxSounds[i]->release(); m_SfxSounds[i] = nullptr; }
        }
        if (m_BgmGroup) { m_BgmGroup->release(); m_BgmGroup = nullptr; }
        if (m_SfxGroup) { m_SfxGroup->release(); m_SfxGroup = nullptr; }
        m_FmodSystem->close();
        m_FmodSystem->release();
        m_FmodSystem = nullptr;
    }

    m_Initialized = false;
    LOG_INFO("[Sys_Audio] OnDestroy");
}

// ============================================================
// 内部方法
// ============================================================

void Sys_Audio::LoadSound(const char* path, bool loop, int index, bool isBgm) {
    if (!path || path[0] == '\0') return;

    std::string fullPath = NCL::Assets::ASSETROOT + path;

    FMOD_MODE mode = FMOD_DEFAULT;
    if (loop) {
        mode |= FMOD_LOOP_NORMAL | FMOD_CREATESTREAM;
    } else {
        mode |= FMOD_LOOP_OFF | FMOD_CREATESAMPLE;
    }

    FMOD::Sound* sound = nullptr;
    FMOD_RESULT result = m_FmodSystem->createSound(fullPath.c_str(), mode, nullptr, &sound);
    if (result != FMOD_OK) {
        LOG_WARN("[Sys_Audio] Failed to load " << fullPath << ": " << FMOD_ErrorString(result));
        return;
    }

    if (isBgm) {
        m_BgmSounds[index] = sound;
    } else {
        m_SfxSounds[index] = sound;
    }
}

void Sys_Audio::StopBgm() {
    if (m_BgmChannel) {
        bool playing = false;
        m_BgmChannel->isPlaying(&playing);
        if (playing) {
            m_BgmChannel->stop();
        }
        m_BgmChannel = nullptr;
    }
}

bool Sys_Audio::PlayBgm(BgmId id) {
    int idx = (int)id;
    if (idx <= 0 || idx >= (int)BgmId::COUNT) return false;
    if (!m_BgmSounds[idx]) {
        LOG_WARN("[Sys_Audio] PlayBgm skipped — no sound loaded for id=" << idx);
        return false;
    }

    FMOD_RESULT result = m_FmodSystem->playSound(m_BgmSounds[idx], m_BgmGroup, false, &m_BgmChannel);
    if (result != FMOD_OK) {
        LOG_WARN("[Sys_Audio] PlayBgm failed for id=" << idx << ": " << FMOD_ErrorString(result));
        m_BgmChannel = nullptr;
        return false;
    }
    return true;
}

void Sys_Audio::PlaySfx(SfxId id) {
    int idx = (int)id;
    if (idx <= 0 || idx >= (int)SfxId::COUNT) return;
    if (!m_SfxSounds[idx]) {
        LOG_WARN("[Sys_Audio] PlaySfx skipped — no sound loaded for id=" << idx << " (path may be empty/reserved)");
        return;
    }

    FMOD::Channel* ch = nullptr;
    FMOD_RESULT result = m_FmodSystem->playSound(m_SfxSounds[idx], m_SfxGroup, false, &ch);
    if (result != FMOD_OK) {
        LOG_WARN("[Sys_Audio] PlaySfx failed for id=" << idx << ": " << FMOD_ErrorString(result));
    }
}

} // namespace ECS
